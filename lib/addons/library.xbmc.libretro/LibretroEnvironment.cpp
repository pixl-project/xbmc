/*
 *      Copyright (C) 2014 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "LibretroEnvironment.h"
#include "ClientBridge.h"
#include "FrontendBridge.h"
#include "libretro.h"
#include "LibretroDLL.h"
#include "libXBMC_game.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

using namespace ADDON;
using namespace LIBRETRO;

#define DEFAULT_NOTIFICATION_TIME_MS  3000 // Time to display toast dialogs, from AddonCallbacksAddon.cpp

CHelper_libXBMC_addon* CLibretroEnvironment::m_xbmc = NULL;
CHelper_libXBMC_game*  CLibretroEnvironment::m_frontend = NULL;
CClientBridge*         CLibretroEnvironment::m_clientBridge = NULL;

double CLibretroEnvironment::m_fps = 0.0;
bool   CLibretroEnvironment::m_bFramerateKnown = false;

void CLibretroEnvironment::Initialize(CHelper_libXBMC_addon* xbmc, CHelper_libXBMC_game* frontend, CLibretroDLL* client, CClientBridge *clientBridge)
{
  m_xbmc = xbmc;
  m_frontend = frontend;
  m_clientBridge = clientBridge;

  // Install environment callback
  client->retro_set_environment(EnvironmentCallback);
}

void CLibretroEnvironment::UpdateFramerate(double fps)
{
  m_fps = fps;
  m_bFramerateKnown = true;
}

bool CLibretroEnvironment::EnvironmentCallback(unsigned int cmd, void *data)
{
  if (!m_frontend || !m_clientBridge)
    return false;

  switch (cmd)
  {
  case RETRO_ENVIRONMENT_SET_ROTATION:
    {
      const unsigned* typedData = reinterpret_cast<const unsigned*>(data);
      if (typedData)
        m_frontend->EnvironmentSetRotation(static_cast<GAME_ROTATION>(*typedData));
      break;
    }
  case RETRO_ENVIRONMENT_GET_OVERSCAN:
    {
      bool* typedData = reinterpret_cast<bool*>(data);
      if (typedData)
        *typedData = m_frontend->EnvironmentGetOverscan();
      break;
    }
  case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    {
      bool* typedData = reinterpret_cast<bool*>(data);
      if (typedData)
        *typedData = m_frontend->EnvironmentCanDupe();
      break;
    }
  case RETRO_ENVIRONMENT_SET_MESSAGE:
    {
      // Sets a message to be displayed. Generally not for trivial messages.
      const retro_message* typedData = reinterpret_cast<const retro_message*>(data);
      if (typedData)
      {
        const char* msg = typedData->msg;

        // Convert frame count to duration
        unsigned int notificationTimeMs = DEFAULT_NOTIFICATION_TIME_MS;
        if (m_bFramerateKnown)
          notificationTimeMs = (unsigned int)(1000 * typedData->frames / m_fps);
        
        // TODO: Include notification time parameter in QueueNotification()
        m_xbmc->QueueNotification(QUEUE_INFO, msg);
      }
      break;
    }
  case RETRO_ENVIRONMENT_SHUTDOWN:
    {
      m_frontend->ShutdownFrontend();
      break;
    }
  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    {
      const unsigned* typedData = reinterpret_cast<const unsigned*>(data);
      if (typedData)
        m_frontend->EnvironmentSetPerformanceLevel(*typedData);
      break;
    }
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    {
      const char** typedData = reinterpret_cast<const char**>(data);
      if (typedData)
      {
        char* systemDir = m_frontend->EnvironmentGetSystemDirectory();
        if (systemDir)
        {
          static char systemDirBuffer[8192];
          memset(systemDirBuffer, 0, sizeof(systemDirBuffer));
          strncpy(systemDirBuffer, systemDir, sizeof(systemDirBuffer));
          *typedData = systemDirBuffer;
          m_xbmc->FreeString(systemDir);
        }
        else
        {
          *typedData = NULL;
        }
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
      const retro_pixel_format* typedData = reinterpret_cast<const retro_pixel_format*>(data);
      if (!typedData)
        return false;

      if (!m_frontend->EnvironmentSetPixelFormat((GAME_PIXEL_FORMAT)*typedData))
        return false;

      break;
    }
  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    {
      const retro_input_descriptor* typedData = reinterpret_cast<const retro_input_descriptor*>(data);
      size_t count = 0;
      for (const retro_input_descriptor *descriptor = typedData; descriptor && descriptor->description; descriptor++)
        count++;

      if (count)
      {
        // Translate struct
        game_input_descriptor* descriptors = new game_input_descriptor[count];
        for (unsigned int i = 0; i < count; i++)
        {
          descriptors[i].port        = typedData->port;
          descriptors[i].device      = typedData->device;
          descriptors[i].index       = typedData->index;
          descriptors[i].id          = typedData->id;
          descriptors[i].description = typedData->description;
        }
        m_frontend->EnvironmentSetInputDescriptors(descriptors, count);
        delete descriptors;
      }

      break;
    }
  case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
    {
      const retro_keyboard_callback* typedData = reinterpret_cast<const retro_keyboard_callback*>(data);
      if (typedData)
      {
        // Store callback from libretro client
        m_clientBridge->m_retro_keyboard_event = typedData->callback;
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    {
      const retro_disk_control_callback *typedData = reinterpret_cast<const retro_disk_control_callback*>(data);
      if (typedData)
      {
        // Store callbacks from libretro client
        m_clientBridge->m_retro_disk_set_eject_state     = typedData->set_eject_state;
        m_clientBridge->m_retro_disk_get_eject_state     = typedData->get_eject_state;
        m_clientBridge->m_retro_disk_get_image_index     = typedData->get_image_index;
        m_clientBridge->m_retro_disk_set_image_index     = typedData->set_image_index;
        m_clientBridge->m_retro_disk_get_num_images      = typedData->get_num_images;
        m_clientBridge->m_retro_disk_replace_image_index = typedData->replace_image_index;
        m_clientBridge->m_retro_disk_add_image_index     = typedData->add_image_index;
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
      retro_hw_render_callback* typedData = reinterpret_cast<retro_hw_render_callback*>(data);
      if (typedData)
      {
        // Translate struct and report hw info to frontend
        game_hw_info hw_info;
        hw_info.context_type       = (GAME_HW_CONTEXT_TYPE)typedData->context_type;
        hw_info.depth              = typedData->depth;
        hw_info.stencil            = typedData->stencil;
        hw_info.bottom_left_origin = typedData->bottom_left_origin;
        hw_info.version_major      = typedData->version_major;
        hw_info.version_minor      = typedData->version_minor;
        hw_info.cache_context      = typedData->cache_context;
        hw_info.debug_context      = typedData->debug_context;
        m_frontend->HwSetInfo(&hw_info);

        // Store callbacks from libretro client
        m_clientBridge->m_retro_hw_context_reset   = typedData->context_reset;
        m_clientBridge->m_retro_hw_context_destroy = typedData->context_destroy;

        // Expose frontend callbacks to libretro client
        typedData->get_current_framebuffer = CFrontendBridge::HwGetCurrentFramebuffer;
        typedData->get_proc_address        = CFrontendBridge::HwGetProcAddress;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
      retro_variable* typedData = reinterpret_cast<retro_variable*>(data);
      if (typedData)
      {
        // Translate struct
        game_variable variable;
        variable.key   = typedData->key;
        variable.value = typedData->value;
        m_frontend->EnvironmentGetVariable(&variable);
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_VARIABLES:
    {
      const retro_variable* typedData = reinterpret_cast<const retro_variable*>(data);
      size_t count = 0;
      for (const retro_variable* variable = typedData; variable && variable->key && variable->value; variable++)
        count++;
      
      if (count)
      {
        // Translate struct
        game_variable* variables = new game_variable[count];
        for (unsigned int i = 0; i < count; i++)
        {
          variables[i].key   = typedData[i].key;
          variables[i].value = typedData[i].value;
        }
        m_frontend->EnvironmentSetVariables(variables, count);
        delete variables;
      }

      break;
    }
  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    {
      bool* typedData = reinterpret_cast<bool*>(data);
      if (typedData)
        *typedData = m_frontend->EnvironmentGetVariableUpdate();
      break;
    }
  case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    {
      const bool* typedData = reinterpret_cast<const bool*>(data);
      if (typedData)
        m_frontend->EnvironmentSetSupportNoGame(*typedData);
      break;
    }
  case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
    {
      const char** typedData = reinterpret_cast<const char**>(data);
      if (typedData)
      {
        char* libretroPath = m_frontend->EnvironmentGetLibretroPath();
        if (libretroPath)
        {
          static char libretroPathBuffer[8192];
          memset(libretroPathBuffer, 0, sizeof(libretroPathBuffer));
          strncpy(libretroPathBuffer, libretroPath, sizeof(libretroPathBuffer));
          *typedData = libretroPathBuffer;
          m_xbmc->FreeString(libretroPath);
        }
        else
        {
          *typedData = NULL;
        }
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
    {
      const retro_audio_callback *typedData = reinterpret_cast<const retro_audio_callback*>(data);
      if (typedData)
      {
        // Store callbacks from libretro client
        m_clientBridge->m_retro_audio_callback           = typedData->callback;
        m_clientBridge->m_retro_audio_set_state_callback = typedData->set_state;
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
    {
      const retro_frame_time_callback *typedData = reinterpret_cast<const retro_frame_time_callback*>(data);
      if (typedData)
      {
        // Store callback from libretro client
        m_clientBridge->m_retro_frame_time_callback = typedData->callback;

        // Report frame time reference
        m_frontend->FrameTimeSetReference(typedData->reference);
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    {
      retro_rumble_interface* typedData = reinterpret_cast<retro_rumble_interface*>(data);
      if (typedData)
      {
        // Expose callback to libretro core
        typedData->set_rumble_state = CFrontendBridge::RumbleSetState;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
    {
      uint64_t* typedData = reinterpret_cast<uint64_t*>(data);
      if (typedData)
        *typedData = m_frontend->InputGetDeviceCapabilities();
      break;
    }
  case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
    {
      retro_sensor_interface* typedData = reinterpret_cast<retro_sensor_interface*>(data);
      if (typedData)
      {
        // Expose callbacks to libretro core
        typedData->set_sensor_state = CFrontendBridge::SensorSetState;
        typedData->get_sensor_input = CFrontendBridge::SensorGetInput;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
    {
      retro_camera_callback* typedData = reinterpret_cast<retro_camera_callback*>(data);
      if (typedData)
      {
        // Translate struct and report camera info to frontend
        game_camera_info camera_info;
        camera_info.caps   = typedData->caps;
        camera_info.width  = typedData->width;
        camera_info.height = typedData->height;
        m_frontend->CameraSetInfo(&camera_info);

        // Store callbacks from libretro core
        m_clientBridge->m_retro_camera_frame_raw_buffer     = typedData->frame_raw_framebuffer;
        m_clientBridge->m_retro_camera_frame_opengl_texture = typedData->frame_opengl_texture;
        m_clientBridge->m_retro_camera_initialized          = typedData->initialized;
        m_clientBridge->m_retro_camera_deinitialized        = typedData->deinitialized;

        // Expose callbacks to libretro core
        typedData->start = CFrontendBridge::CameraStart;
        typedData->stop  = CFrontendBridge::CameraStop;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    {
      retro_log_callback* typedData = reinterpret_cast<retro_log_callback*>(data);
      if (typedData)
      {
        // Expose callback to libretro core
        typedData->log = CFrontendBridge::LogFrontend; // libretro logging forwards to XBMC add-on log function
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    {
      retro_perf_callback* typedData = reinterpret_cast<retro_perf_callback*>(data);
      if (typedData)
      {
        // Expose callbacks to libretro core
        typedData->get_time_usec    = CFrontendBridge::PerfGetTimeUsec;
        typedData->get_cpu_features = CFrontendBridge::PerfGetCpuFeatures;
        typedData->get_perf_counter = CFrontendBridge::PerfGetCounter;
        typedData->perf_register    = CFrontendBridge::PerfRegister;
        typedData->perf_start       = CFrontendBridge::PerfStart;
        typedData->perf_stop        = CFrontendBridge::PerfStop;
        typedData->perf_log         = CFrontendBridge::PerfLog;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
    {
      retro_location_callback* typedData = reinterpret_cast<retro_location_callback*>(data);
      if (typedData)
      {
        // Expose callbacks to libretro core
        typedData->start         = CFrontendBridge::LocationStart;
        typedData->stop          = CFrontendBridge::LocationStop;
        typedData->get_position  = CFrontendBridge::LocationGetPosition;
        typedData->set_interval  = CFrontendBridge::LocationSetInterval;
        typedData->initialized   = CFrontendBridge::LocationInitialized;
        typedData->deinitialized = CFrontendBridge::LocationDeinitialized;
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY:
    {
      const char** typedData = reinterpret_cast<const char**>(data);
      if (typedData)
      {
        char* contentDir = m_frontend->EnvironmentGetLibretroPath();
        if (contentDir)
        {
          static char contentDirBuffer[8192];
          memset(contentDirBuffer, 0, sizeof(contentDirBuffer));
          strncpy(contentDirBuffer, contentDir, sizeof(contentDirBuffer));
          *typedData = contentDirBuffer;
          m_xbmc->FreeString(contentDir);
        }
        else
        {
          *typedData = NULL;
        }
      }
      break;
    }
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    {
      const char** typedData = reinterpret_cast<const char**>(data);
      if (typedData)
      {
        char* saveDir = m_frontend->EnvironmentGetLibretroPath();
        if (saveDir)
        {
          static char saveDirBuffer[8192];
          memset(saveDirBuffer, 0, sizeof(saveDirBuffer));
          strncpy(saveDirBuffer, saveDir, sizeof(saveDirBuffer));
          *typedData = saveDirBuffer;
          m_xbmc->FreeString(saveDir);
        }
        else
        {
          *typedData = NULL;
        }
      }
      break;
    }
  case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
      const retro_system_av_info* typedData = reinterpret_cast<const retro_system_av_info*>(data);
      if (!typedData)
        return false;

      // Translate struct
      game_system_av_info info;
      info.geometry.base_width   = typedData->geometry.base_width;
      info.geometry.base_height  = typedData->geometry.base_height;
      info.geometry.max_width    = typedData->geometry.max_width;
      info.geometry.max_height   = typedData->geometry.max_height;
      info.geometry.aspect_ratio = typedData->geometry.aspect_ratio;
      info.timing.fps            = typedData->timing.fps;
      info.timing.sample_rate    = typedData->timing.sample_rate;
      if (!m_frontend->EnvironmentSetSystemAvInfo(&info))
        return false; // Frontend does not acknowledge a changed av_info struct
      else
        UpdateFramerate(info.timing.fps); // Record the new framerate

      break;
    }
  }

  return true;
}
