// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_id.h"

#include <cstdint>

#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;
const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;
const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kUsbMicId = 10005;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const AudioNodeInfo kInternalMic = {true, kInternalMicId, "Fake Mic",
                                    "INTERNAL_MIC", "Internal Mic"};

const AudioNodeInfo kHeadphone = {false, kHeadphoneId, "Fake Headphone",
                                  "HEADPHONE", "Headphone"};

const AudioNodeInfo kUSBMic = {true, kUsbMicId, "Fake USB Mic", "USB",
                               "USB Microphone"};

AudioDevice CreateAudioDevice(const AudioNodeInfo& info, int version) {
  return AudioDevice(AudioNode(
      info.is_input, info.id, /*has_v2_stable_device_id=*/version == 2,
      /*stable_device_id_v1=*/info.id,
      /*stable_device_id_v2=*/version == 1 ? 0 : info.id ^ 0xFF,
      info.device_name, info.type, info.name, false, 0,
      info.is_input ? kInputMaxSupportedChannels : kOutputMaxSupportedChannels,
      info.is_input ? kInputAudioEffect : kOutputAudioEffect,
      info.is_input ? kInputNumberOfVolumeSteps : kOutputNumberOfVolumeSteps));
}

}  // namespace

class AudioDeviceIdTest : public AudioDeviceSelectionTestBase {};

TEST_F(AudioDeviceIdTest, GetDeviceIdString) {
  struct {
    const AudioNodeInfo audio_node_info;
    int version;
    const std::string expected_id;
  } items[] = {
      {kInternalMic, 1, "10003 : 1"}, {kInternalMic, 2, "2 : 10220 : 1"},
      {kHeadphone, 1, "10002 : 0"},   {kHeadphone, 2, "2 : 10221 : 0"},
      {kUSBMic, 1, "10005 : 1"},      {kUSBMic, 2, "2 : 10218 : 1"}};

  for (const auto& item : items) {
    EXPECT_EQ(GetDeviceIdString(
                  CreateAudioDevice(item.audio_node_info, item.version)),
              item.expected_id);
  }
}

// Verify GetDeviceSetIdString() can convert AudioDeviceList
// to string containing comma separated set of versioned device IDs.
TEST_F(AudioDeviceIdTest, GetDeviceSetIdString) {
  struct {
    const AudioDeviceList audio_device_list;
    const std::string expected_id;
  } items[] = {
      {{}, ""},
      {{CreateAudioDevice(kInternalMic, 1)}, "10003 : 1"},
      {{CreateAudioDevice(kInternalMic, 1), CreateAudioDevice(kInternalMic, 1)},
       "10003 : 1"},
      {{CreateAudioDevice(kInternalMic, 1), CreateAudioDevice(kHeadphone, 1)},
       "10002 : 0,10003 : 1"},
      {{CreateAudioDevice(kInternalMic, 1), CreateAudioDevice(kHeadphone, 1),
        CreateAudioDevice(kUSBMic, 1)},
       "10002 : 0,10003 : 1,10005 : 1"},
      {{CreateAudioDevice(kHeadphone, 1), CreateAudioDevice(kInternalMic, 1),
        CreateAudioDevice(kUSBMic, 1)},
       "10002 : 0,10003 : 1,10005 : 1"},
      {{CreateAudioDevice(kUSBMic, 1), CreateAudioDevice(kHeadphone, 1),
        CreateAudioDevice(kInternalMic, 1)},
       "10002 : 0,10003 : 1,10005 : 1"}};

  for (const auto& item : items) {
    EXPECT_EQ(GetDeviceSetIdString(item.audio_device_list), item.expected_id);
  }
}

TEST_F(AudioDeviceIdTest, ParseDeviceId) {
  struct {
    const std::string id_string;
    const std::optional<uint64_t> expected_id;
  } items[] = {
      {"", std::nullopt},           {"2", 2},         {"10003 : 1", 10003},
      {"2 : 10220 : 1", 10220},     {"10220", 10220}, {"a120000", std::nullopt},
      {"2 : a120000", std::nullopt}};

  for (const auto& item : items) {
    EXPECT_EQ(ParseDeviceId(item.id_string), item.expected_id);
  }
}

}  // namespace ash
