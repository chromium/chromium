// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_AUDIO_NODE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_AUDIO_NODE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"

namespace ash {

constexpr int32_t NUMBER_OF_VOLUME_STEPS_DEFAULT = 25;

// Structure to hold AudioNode data received from cras.
struct COMPONENT_EXPORT(DBUS_AUDIO) AudioNode {
  bool is_input = false;
  uint64_t id = 0;
  bool has_v2_stable_device_id = false;
  uint64_t stable_device_id_v1 = 0;
  uint64_t stable_device_id_v2 = 0;
  std::string device_name;
  std::string type;
  std::string name;
  bool active = false;
  // Time that the node was plugged in.
  uint64_t plugged_time = 0;
  // Max supported channel count of the device for the node.
  uint32_t max_supported_channels = 0;
  // Bit-wise audio effect support information.
  uint32_t audio_effect = 0;
  // The number of volume steps that can be adjusted for the node.
  // Mainly used to calculate the percentage of playback volume change.
  // e.g. number_of_volume_steps=25 volume will change 4% (100%/25=4%) for one
  // volume change event. If this value is set to 0, indicates that the value
  // for this node is invalid. Currently all input nodes this value is invalid
  // (0), all output nodes should get a valid number (>0) if output nodes
  // somehow get a number_of_volume_steps <=0, use 25 as default value.
  int32_t number_of_volume_steps = 0;
  AudioNode();
  AudioNode(bool is_input,
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
            int32_t number_of_volume_steps);
  AudioNode(const AudioNode& other);
  ~AudioNode();

  std::string ToString() const;
  int StableDeviceIdVersion() const;
  uint64_t StableDeviceId() const;
};

typedef std::vector<AudioNode> AudioNodeList;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_AUDIO_NODE_H_
