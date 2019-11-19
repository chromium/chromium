// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_device.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace chromeos {

namespace {

// Get the priority for a particular device type. The priority returned
// will be between 0 to 3, the higher number meaning a higher priority.
uint8_t GetDevicePriority(AudioDeviceType type, bool is_input) {
  switch (type) {
    case AUDIO_TYPE_HEADPHONE:
    case AUDIO_TYPE_LINEOUT:
    case AUDIO_TYPE_MIC:
    case AUDIO_TYPE_USB:
    case AUDIO_TYPE_BLUETOOTH:
      return 3;
    case AUDIO_TYPE_HDMI:
      return 2;
    case AUDIO_TYPE_INTERNAL_SPEAKER:
    case AUDIO_TYPE_INTERNAL_MIC:
    case AUDIO_TYPE_FRONT_MIC:
      return 1;
    // Lower the priority of bluetooth mic to prevent unexpected bad eperience
    // to user because of bluetooth audio profile switching. Make priority to
    // zero so this mic will never be automatically chosen.
    case AUDIO_TYPE_BLUETOOTH_NB_MIC:
    // Rear mic should have priority lower than front mic to prevent poor
    // quality input caused by accidental selecting to rear side mic.
    case AUDIO_TYPE_REAR_MIC:
    case AUDIO_TYPE_KEYBOARD_MIC:
    case AUDIO_TYPE_HOTWORD:
    case AUDIO_TYPE_POST_MIX_LOOPBACK:
    case AUDIO_TYPE_POST_DSP_LOOPBACK:
    case AUDIO_TYPE_OTHER:
    default:
      return 0;
  }
}

}  // namespace

// static
std::string AudioDevice::GetTypeString(AudioDeviceType type) {
  switch (type) {
    case AUDIO_TYPE_HEADPHONE:
      return "HEADPHONE";
    case AUDIO_TYPE_MIC:
      return "MIC";
    case AUDIO_TYPE_USB:
      return "USB";
    case AUDIO_TYPE_BLUETOOTH:
      return "BLUETOOTH";
    case AUDIO_TYPE_BLUETOOTH_NB_MIC:
      return "BLUETOOTH_NB_MIC";
    case AUDIO_TYPE_HDMI:
      return "HDMI";
    case AUDIO_TYPE_INTERNAL_SPEAKER:
      return "INTERNAL_SPEAKER";
    case AUDIO_TYPE_INTERNAL_MIC:
      return "INTERNAL_MIC";
    case AUDIO_TYPE_FRONT_MIC:
      return "FRONT_MIC";
    case AUDIO_TYPE_REAR_MIC:
      return "REAR_MIC";
    case AUDIO_TYPE_KEYBOARD_MIC:
      return "KEYBOARD_MIC";
    case AUDIO_TYPE_HOTWORD:
      return "HOTWORD";
    case AUDIO_TYPE_LINEOUT:
      return "LINEOUT";
    case AUDIO_TYPE_POST_MIX_LOOPBACK:
      return "POST_MIX_LOOPBACK";
    case AUDIO_TYPE_POST_DSP_LOOPBACK:
      return "POST_DSP_LOOPBACK";
    case AUDIO_TYPE_OTHER:
    default:
      return "OTHER";
  }
}

// static
AudioDeviceType AudioDevice::GetAudioType(
    const std::string& node_type) {
  if (node_type.find("HEADPHONE") != std::string::npos)
    return AUDIO_TYPE_HEADPHONE;
  else if (node_type.find("INTERNAL_MIC") != std::string::npos)
    return AUDIO_TYPE_INTERNAL_MIC;
  else if (node_type.find("FRONT_MIC") != std::string::npos)
    return AUDIO_TYPE_FRONT_MIC;
  else if (node_type.find("REAR_MIC") != std::string::npos)
    return AUDIO_TYPE_REAR_MIC;
  else if (node_type.find("KEYBOARD_MIC") != std::string::npos)
    return AUDIO_TYPE_KEYBOARD_MIC;
  else if (node_type.find("BLUETOOTH_NB_MIC") != std::string::npos)
    return AUDIO_TYPE_BLUETOOTH_NB_MIC;
  else if (node_type.find("MIC") != std::string::npos)
    return AUDIO_TYPE_MIC;
  else if (node_type.find("USB") != std::string::npos)
    return AUDIO_TYPE_USB;
  else if (node_type.find("BLUETOOTH") != std::string::npos)
    return AUDIO_TYPE_BLUETOOTH;
  else if (node_type.find("HDMI") != std::string::npos)
    return AUDIO_TYPE_HDMI;
  else if (node_type.find("INTERNAL_SPEAKER") != std::string::npos)
    return AUDIO_TYPE_INTERNAL_SPEAKER;
  // TODO(hychao): Remove the 'AOKR' matching line after CRAS switches
  // node type naming to 'HOTWORD'.
  else if (node_type.find("AOKR") != std::string::npos)
    return AUDIO_TYPE_HOTWORD;
  else if (node_type.find("HOTWORD") != std::string::npos)
    return AUDIO_TYPE_HOTWORD;
  else if (node_type.find("LINEOUT") != std::string::npos)
    return AUDIO_TYPE_LINEOUT;
  else if (node_type.find("POST_MIX_LOOPBACK") != std::string::npos)
    return AUDIO_TYPE_POST_MIX_LOOPBACK;
  else if (node_type.find("POST_DSP_LOOPBACK") != std::string::npos)
    return AUDIO_TYPE_POST_DSP_LOOPBACK;
  else
    return AUDIO_TYPE_OTHER;
}

AudioDevice::AudioDevice() = default;

AudioDevice::AudioDevice(const AudioNode& node) {
  is_input = node.is_input;
  id = node.id;
  stable_device_id_version = node.StableDeviceIdVersion();
  stable_device_id = node.StableDeviceId();
  if (stable_device_id_version == 2)
    deprecated_stable_device_id = node.stable_device_id_v1;
  type = GetAudioType(node.type);
  if (!node.name.empty() && node.name != "(default)")
    display_name = node.name;
  else
    display_name = node.device_name;
  device_name = node.device_name;
  mic_positions = node.mic_positions;
  priority = GetDevicePriority(type, node.is_input);
  active = node.active;
  plugged_time = node.plugged_time;
}

AudioDevice::AudioDevice(const AudioDevice& other) = default;

std::string AudioDevice::ToString() const {
  if (stable_device_id_version == 0) {
    return "Null device";
  }

  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = 0x%" PRIx64 " ",
                      id);
  base::StringAppendF(&result, "stable_device_id_version = %d",
                      stable_device_id_version);
  base::StringAppendF(&result, "stable_device_id = 0x%" PRIx64 " ",
                      stable_device_id);
  base::StringAppendF(&result, "deprecated_stable_device_id = 0x%" PRIx64 " ",
                      deprecated_stable_device_id);
  base::StringAppendF(&result, "display_name = %s ", display_name.c_str());
  base::StringAppendF(&result,
                      "device_name = %s ",
                      device_name.c_str());
  base::StringAppendF(&result,
                      "type = %s ",
                      GetTypeString(type).c_str());
  base::StringAppendF(&result,
                      "active = %s ",
                      active ? "true" : "false");
  base::StringAppendF(&result, "plugged_time= %s ",
                      base::NumberToString(plugged_time).c_str());
  base::StringAppendF(&result, "mic_positions = %s ", mic_positions.c_str());

  return result;
}

bool AudioDevice::IsExternalDevice() const {
  if (!is_for_simple_usage())
    return false;

  if (is_input) {
    return !IsInternalMic();
  } else {
    return (type != AUDIO_TYPE_INTERNAL_SPEAKER);
  }
}

bool AudioDevice::IsInternalMic() const {
  switch (type) {
    case AUDIO_TYPE_INTERNAL_MIC:
    case AUDIO_TYPE_FRONT_MIC:
    case AUDIO_TYPE_REAR_MIC:
      return true;
    default:
      return false;
  }
}

}  // namespace chromeos
