// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device.h"

#include <stdint.h>
#include "ash/constants/ash_features.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

namespace {

// Get the priority for a particular device type. The priority returned
// will be between 0 to 3, the higher number meaning a higher priority.
uint8_t GetDevicePriority(AudioDeviceType type, bool is_input) {
  switch (type) {
    case AudioDeviceType::kHeadphone:
    case AudioDeviceType::kLineout:
    case AudioDeviceType::kMic:
    case AudioDeviceType::kUsb:
    case AudioDeviceType::kBluetooth:
      return 3;
    case AudioDeviceType::kHdmi:
      return 2;
    case AudioDeviceType::kInternalSpeaker:
    case AudioDeviceType::kInternalMic:
    case AudioDeviceType::kFrontMic:
      return 1;
    // Lower the priority of bluetooth mic to prevent unexpected bad eperience
    // to user because of bluetooth audio profile switching. Make priority to
    // zero so this mic will never be automatically chosen.
    case AudioDeviceType::kBluetoothNbMic:
    // Rear mic should have priority lower than front mic to prevent poor
    // quality input caused by accidental selecting to rear side mic.
    case AudioDeviceType::kRearMic:
    case AudioDeviceType::kKeyboardMic:
    case AudioDeviceType::kHotword:
    case AudioDeviceType::kPostMixLoopback:
    case AudioDeviceType::kPostDspLoopback:
    case AudioDeviceType::kAlsaLoopback:
    case AudioDeviceType::kOther:
    default:
      return 0;
  }
}

}  // namespace

// static
std::string AudioDevice::GetTypeString(AudioDeviceType type) {
  switch (type) {
    case AudioDeviceType::kHeadphone:
      return "HEADPHONE";
    case AudioDeviceType::kMic:
      return "MIC";
    case AudioDeviceType::kUsb:
      return "USB";
    case AudioDeviceType::kBluetooth:
      return "BLUETOOTH";
    case AudioDeviceType::kBluetoothNbMic:
      return "BLUETOOTH_NB_MIC";
    case AudioDeviceType::kHdmi:
      return "HDMI";
    case AudioDeviceType::kInternalSpeaker:
      return "INTERNAL_SPEAKER";
    case AudioDeviceType::kInternalMic:
      return "INTERNAL_MIC";
    case AudioDeviceType::kFrontMic:
      return "FRONT_MIC";
    case AudioDeviceType::kRearMic:
      return "REAR_MIC";
    case AudioDeviceType::kKeyboardMic:
      return "KEYBOARD_MIC";
    case AudioDeviceType::kHotword:
      return "HOTWORD";
    case AudioDeviceType::kLineout:
      return "LINEOUT";
    case AudioDeviceType::kPostMixLoopback:
      return "POST_MIX_LOOPBACK";
    case AudioDeviceType::kPostDspLoopback:
      return "POST_DSP_LOOPBACK";
    case AudioDeviceType::kAlsaLoopback:
      return "ALSA_LOOPBACK";
    case AudioDeviceType::kOther:
    default:
      return "OTHER";
  }
}

// static
AudioDeviceType AudioDevice::GetAudioType(const std::string& node_type) {
  if (node_type.find("HEADPHONE") != std::string::npos)
    return AudioDeviceType::kHeadphone;
  else if (node_type.find("INTERNAL_MIC") != std::string::npos)
    return AudioDeviceType::kInternalMic;
  else if (node_type.find("FRONT_MIC") != std::string::npos)
    return AudioDeviceType::kFrontMic;
  else if (node_type.find("REAR_MIC") != std::string::npos)
    return AudioDeviceType::kRearMic;
  else if (node_type.find("KEYBOARD_MIC") != std::string::npos)
    return AudioDeviceType::kKeyboardMic;
  else if (node_type.find("BLUETOOTH_NB_MIC") != std::string::npos)
    return AudioDeviceType::kBluetoothNbMic;
  else if (node_type.find("MIC") != std::string::npos)
    return AudioDeviceType::kMic;
  else if (node_type.find("USB") != std::string::npos)
    return AudioDeviceType::kUsb;
  else if (node_type.find("BLUETOOTH") != std::string::npos)
    return AudioDeviceType::kBluetooth;
  else if (node_type.find("HDMI") != std::string::npos)
    return AudioDeviceType::kHdmi;
  else if (node_type.find("INTERNAL_SPEAKER") != std::string::npos)
    return AudioDeviceType::kInternalSpeaker;
  // TODO(hychao): Remove the 'AOKR' matching line after CRAS switches
  // node type naming to 'HOTWORD'.
  else if (node_type.find("AOKR") != std::string::npos)
    return AudioDeviceType::kHotword;
  else if (node_type.find("HOTWORD") != std::string::npos)
    return AudioDeviceType::kHotword;
  else if (node_type.find("LINEOUT") != std::string::npos)
    return AudioDeviceType::kLineout;
  else if (node_type.find("POST_MIX_LOOPBACK") != std::string::npos)
    return AudioDeviceType::kPostMixLoopback;
  else if (node_type.find("POST_DSP_LOOPBACK") != std::string::npos)
    return AudioDeviceType::kPostDspLoopback;
  else if (node_type.find("ALSA_LOOPBACK") != std::string::npos)
    return AudioDeviceType::kAlsaLoopback;
  else
    return AudioDeviceType::kOther;
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
  priority = GetDevicePriority(type, node.is_input);
  active = node.active;
  plugged_time = node.plugged_time;
  max_supported_channels = node.max_supported_channels;
  audio_effect = node.audio_effect;
  number_of_volume_steps = node.number_of_volume_steps;
}

AudioDevice::AudioDevice(const AudioDevice& other) = default;

AudioDevice& AudioDevice::operator=(const AudioDevice& other) = default;

std::string AudioDevice::ToString() const {
  if (stable_device_id_version == 0) {
    return "Null device";
  }

  std::string result;
  base::StringAppendF(&result, "is_input = %s ", is_input ? "true" : "false");
  base::StringAppendF(&result, "id = 0x%" PRIx64 " ", id);
  base::StringAppendF(&result, "stable_device_id_version = %d",
                      stable_device_id_version);
  base::StringAppendF(&result, "stable_device_id = 0x%" PRIx64 " ",
                      stable_device_id);
  base::StringAppendF(&result, "deprecated_stable_device_id = 0x%" PRIx64 " ",
                      deprecated_stable_device_id);
  base::StringAppendF(&result, "display_name = %s ", display_name.c_str());
  base::StringAppendF(&result, "device_name = %s ", device_name.c_str());
  base::StringAppendF(&result, "type = %s ", GetTypeString(type).c_str());
  base::StringAppendF(&result, "active = %s ", active ? "true" : "false");
  base::StringAppendF(&result, "plugged_time = %s ",
                      base::NumberToString(plugged_time).c_str());
  base::StringAppendF(&result, "max_supported_channels = %s ",
                      base::NumberToString(max_supported_channels).c_str());
  base::StringAppendF(&result, "audio_effect = %s ",
                      base::NumberToString(audio_effect).c_str());
  base::StringAppendF(&result, "number_of_volume_steps = %s ",
                      base::NumberToString(number_of_volume_steps).c_str());
  return result;
}

bool AudioDevice::IsExternalDevice() const {
  if (!is_for_simple_usage())
    return false;

  if (is_input) {
    return !IsInternalMic();
  } else {
    return (type != AudioDeviceType::kInternalSpeaker);
  }
}

bool AudioDevice::IsInternalMic() const {
  switch (type) {
    case AudioDeviceType::kInternalMic:
    case AudioDeviceType::kFrontMic:
    case AudioDeviceType::kRearMic:
      return true;
    default:
      return false;
  }
}

bool LessBuiltInPriority(const AudioDevice& a, const AudioDevice& b) {
  if (a.priority < b.priority) {
    return true;
  } else if (b.priority < a.priority) {
    return false;
  } else if (a.plugged_time < b.plugged_time) {
    return true;
  } else {
    return false;
  }
}

bool LessUserPriority(const AudioDevice& a, const AudioDevice& b) {
  if (a.user_priority < b.user_priority) {
    return true;
  } else if (b.user_priority < a.user_priority) {
    return false;
  } else {
    return LessBuiltInPriority(a, b);
  }
}

bool AudioDeviceCompare::operator()(const AudioDevice& a,
                                    const AudioDevice& b) const {
  if (base::FeatureList::IsEnabled(features::kRobustAudioDeviceSelectLogic)) {
    return LessUserPriority(a, b);
  } else {
    return LessBuiltInPriority(a, b);
  }
}

}  // namespace ash
