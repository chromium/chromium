// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_encoding.h"

#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AudioDeviceEncodingTest : public AudioDeviceSelectionTestBase {};

// Test that EncodeAudioDeviceSet encodes an audio device set correctly.
TEST_F(AudioDeviceEncodingTest, EncodeAudioDeviceSet) {
  struct {
    const AudioDeviceList& devices;
    const uint32_t expected_number;
  } items[] = {
      {{}, 0b0},
      {{AudioDevice(NewInputNode("INTERNAL_MIC"))}, 0b1},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("FRONT_MIC"))},
       0b10},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("FRONT_MIC")),
        AudioDevice(NewInputNode("REAR_MIC"))},
       0b11},
      {{AudioDevice(NewOutputNode("INTERNAL_SPEAKER"))}, 1},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("MIC"))},
       0b101},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("USB"))},
       0b10001},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("USB")), AudioDevice(NewInputNode("USB"))},
       0b100001},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("USB")), AudioDevice(NewInputNode("USB")),
        AudioDevice(NewInputNode("USB"))},
       0b110001},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("MIC")), AudioDevice(NewInputNode("USB"))},
       0b10101},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("BLUETOOTH"))},
       0b1000001},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("MIC")), AudioDevice(NewInputNode("USB")),
        AudioDevice(NewInputNode("BLUETOOTH"))},
       0b1010101},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("HDMI"))},
       0b100000001},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("MIC")), AudioDevice(NewInputNode("USB")),
        AudioDevice(NewInputNode("HDMI"))},
       0b100010101},
      {{AudioDevice(NewInputNode("INTERNAL_MIC")),
        AudioDevice(NewInputNode("MIC")), AudioDevice(NewInputNode("USB")),
        AudioDevice(NewInputNode("BLUETOOTH")),
        AudioDevice(NewInputNode("HDMI"))},
       0b101010101},
  };

  for (const auto& item : items) {
    EXPECT_EQ(EncodeAudioDeviceSet(item.devices), item.expected_number);
  }
}

}  // namespace ash
