// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/audio_node.h"

#include <stdint.h>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

AudioNode::AudioNode() = default;

AudioNode::AudioNode(bool is_input,
                     uint64_t id,
                     bool has_v2_stable_device_id,
                     uint64_t stable_device_id_v1,
                     uint64_t stable_device_id_v2,
                     std::string device_name,
                     std::string type,
                     std::string name,
                     bool active,
                     uint64_t plugged_time,
                     uint32_t max_supported_channels,
                     uint32_t audio_effect,
                     int32_t number_of_volume_steps)
    : is_input(is_input),
      id(id),
      has_v2_stable_device_id(has_v2_stable_device_id),
      stable_device_id_v1(stable_device_id_v1),
      stable_device_id_v2(stable_device_id_v2),
      device_name(device_name),
      type(type),
      name(name),
      active(active),
      plugged_time(plugged_time),
      max_supported_channels(max_supported_channels),
      audio_effect(audio_effect),
      number_of_volume_steps(number_of_volume_steps) {
  DCHECK(!(audio_effect & (~(cras::EFFECT_TYPE_NOISE_CANCELLATION |
                             cras::EFFECT_TYPE_HFP_MIC_SR |
                             cras::EFFECT_TYPE_STYLE_TRANSFER))));
  if (!is_input && number_of_volume_steps <= 0) {
    this->number_of_volume_steps = NUMBER_OF_VOLUME_STEPS_DEFAULT;
  }
}

AudioNode::AudioNode(const AudioNode& other) = default;

AudioNode::~AudioNode() = default;

std::string AudioNode::ToString() const {
  std::string result;
  base::StringAppendF(&result, "is_input = %s ", is_input ? "true" : "false");
  base::StringAppendF(&result, "id = 0x%" PRIx64 " ", id);
  base::StringAppendF(&result, "stable_device_id_version = %d",
                      StableDeviceIdVersion());
  base::StringAppendF(&result, "stable_device_id_v1 = 0x%" PRIx64 " ",
                      stable_device_id_v1);
  base::StringAppendF(&result, "stable_device_id_v2 = 0x%" PRIx64 " ",
                      stable_device_id_v2);
  base::StringAppendF(&result, "device_name = %s ", device_name.c_str());
  base::StringAppendF(&result, "type = %s ", type.c_str());
  base::StringAppendF(&result, "name = %s ", name.c_str());
  base::StringAppendF(&result, "active = %s ", active ? "true" : "false");
  base::StringAppendF(&result, "plugged_time= %s ",
                      base::NumberToString(plugged_time).c_str());
  base::StringAppendF(&result, "max_supported_channels= %s ",
                      base::NumberToString(max_supported_channels).c_str());
  base::StringAppendF(&result, "audio_effect = 0x%" PRIx32 " ", audio_effect);
  base::StringAppendF(&result, "number_of_volume_steps= %s",
                      base::NumberToString(number_of_volume_steps).c_str());
  return result;
}

int AudioNode::StableDeviceIdVersion() const {
  return has_v2_stable_device_id ? 2 : 1;
}

uint64_t AudioNode::StableDeviceId() const {
  return has_v2_stable_device_id ? stable_device_id_v2 : stable_device_id_v1;
}

}  // namespace ash
