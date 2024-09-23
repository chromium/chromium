// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"

namespace ash {

// This enum is used in histograms, do not remove/renumber entries. If you're
// adding to this enum, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml.
//
// Originally ordered from the highest priority to the lowest.
enum class AudioDeviceType {
  kHeadphone,
  kMic,
  kUsb,
  kBluetooth,
  kBluetoothNbMic,
  kHdmi,
  kInternalSpeaker,
  kInternalMic,
  kFrontMic,
  kRearMic,
  kKeyboardMic,
  kHotword,
  kLineout,
  kPostMixLoopback,
  kPostDspLoopback,
  kAlsaLoopback,
  kOther,
  kMaxValue = kOther,
};

// Default value of user priority preference.
const int kUserPriorityNone = 0;
// Min value of user priority preference.
const int kUserPriorityMin = 1;

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) AudioDevice {
  AudioDevice();
  explicit AudioDevice(const AudioNode& node);
  AudioDevice(const AudioDevice& other);
  AudioDevice& operator=(const AudioDevice& other);
  std::string ToString() const;

  // Converts between the string type sent via D-Bus and AudioDeviceType.
  // Static so they can be used by tests.
  static std::string GetTypeString(AudioDeviceType type);
  static AudioDeviceType GetAudioType(const std::string& node_type);

  // Indicates that an input or output audio device is for simple usage like
  // playback or recording for user. In contrast, audio device such as
  // loopback, always on keyword recognition (HOTWORD), and keyboard mic are
  // not for simple usage.
  // One special case is ALSA loopback device, which will only exist under
  // testing, and we want it visible to users for e2e tests.
  bool is_for_simple_usage() const {
    return (type == AudioDeviceType::kHeadphone ||
            type == AudioDeviceType::kInternalMic ||
            type == AudioDeviceType::kFrontMic ||
            type == AudioDeviceType::kRearMic ||
            type == AudioDeviceType::kMic || type == AudioDeviceType::kUsb ||
            type == AudioDeviceType::kBluetooth ||
            type == AudioDeviceType::kBluetoothNbMic ||
            type == AudioDeviceType::kHdmi ||
            type == AudioDeviceType::kInternalSpeaker ||
            type == AudioDeviceType::kLineout ||
            type == AudioDeviceType::kAlsaLoopback);
  }

  // Indicates if a device has privilege. System will automatically
  // activate those devices when they are connected, disregarding previously
  // saved user preferences. In other words, having privilege overrides the
  // priority stored in user preferences.
  bool has_privilege() const {
    return type == AudioDeviceType::kHeadphone ||
           type == AudioDeviceType::kMic ||
           type == AudioDeviceType::kBluetooth ||
           type == AudioDeviceType::kBluetoothNbMic;
  }

  bool IsExternalDevice() const;

  bool IsInternalMic() const;

  bool IsInternalSpeaker() const;

  bool is_input = false;

  // Id of this audio device. The legacy |id| is assigned to be unique everytime
  // when each device got plugged, so that the same physical device will have
  // a different id after unplug then re-plug.
  // The |stable_device_id| is designed to be persistent across system reboot
  // and plug/unplug for the same physical device. It is guaranteed that
  // different type of hardware has different |stable_device_id|, but not
  // guaranteed to be different between the same kind of audio device, e.g
  // USB headset. |id| and |stable_device_id| can be used together to achieve
  // various goals.
  // Note that because algorithm used to determine |stable_device_id| changed in
  // system code, |stable_device_id_version| and |deprecated_stable_device_id|
  // have been introduced - to ensure backward compatibility until persisted
  // references to stable device ID have been updated where needed.
  // |stable_device_id_version| is the version of stable device ID set in
  // |stable_device_id|. If version is set to 2, |deprecated_stable_device_id|
  // will contain deprecated, v1 stable device id version.
  uint64_t id = 0;
  int stable_device_id_version = 0;
  uint64_t stable_device_id = 0;
  uint64_t deprecated_stable_device_id = 0;
  std::string display_name;
  std::string device_name;
  AudioDeviceType type = AudioDeviceType::kOther;
  uint8_t priority = 0;
  bool active = false;
  uint64_t plugged_time = 0;
  uint32_t max_supported_channels = 0;
  uint32_t audio_effect = 0;
  int32_t number_of_volume_steps = 0;
  // The larger value means the higher priority.
  uint32_t user_priority = kUserPriorityNone;
};

typedef std::vector<AudioDevice> AudioDeviceList;
typedef std::map<uint64_t, AudioDevice> AudioDeviceMap;

// Compare device user priority first. If tie, compare them with the
// LessBuiltInPriority().
bool LessUserPriority(const AudioDevice& a, const AudioDevice& b);

// Rules used to discern which device is higher:
// 1.) Device Type:
//       [Headphones/USB/Bluetooth > HDMI > Internal Speakers]
//       [External Mic/USB Mic/Bluetooth > Internal Mic]
// 2.) Device Plugged in Time:
//       [Later > Earlier]
bool LessBuiltInPriority(const AudioDevice& a, const AudioDevice& b);

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) AudioDeviceCompare {
  bool operator()(const AudioDevice& a, const AudioDevice& b) const;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_H_
