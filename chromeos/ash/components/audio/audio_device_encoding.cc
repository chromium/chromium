// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_encoding.h"

#include <bitset>

#include "base/check.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

namespace {

// Bit size for a set of audio devices.
const size_t kSingleDeviceSetBitSize = 14;

// Gets the bit position of all current input/output device set.
// Do not change position since it's used for histograms.
size_t GetBitPositionFromDeviceType(AudioDeviceType type) {
  switch (type) {
    case AudioDeviceType::kInternalMic:
    case AudioDeviceType::kFrontMic:
    case AudioDeviceType::kRearMic:
    case AudioDeviceType::kInternalSpeaker:
      return 0;
    case AudioDeviceType::kMic:
    case AudioDeviceType::kHeadphone:
    case AudioDeviceType::kLineout:
      return 2;
    case AudioDeviceType::kUsb:
      return 4;
    case AudioDeviceType::kBluetooth:
    case AudioDeviceType::kBluetoothNbMic:
      return 6;
    case AudioDeviceType::kHdmi:
      return 8;
    default:
      // All other types.
      return 10;
  }
}

// Counts the device to the given val.
void CountDevice(std::bitset<kSingleDeviceSetBitSize>& val,
                 const AudioDevice& device) {
  size_t pos = GetBitPositionFromDeviceType(device.type);
  // The position for each device type should be an even number.
  CHECK(pos % 2 == 0);

  // If current bit is off, turn it on no matter next bit is on or off.
  if (!val.test(pos)) {
    val.set(pos, true);
  } else if (!val.test(pos + 1)) {
    // If current bit is on and next bit is off, flip both of them.
    val.flip(pos);
    val.flip(pos + 1);
  }
}

}  // namespace

uint32_t EncodeAudioDeviceSet(const AudioDeviceList& devices) {
  std::bitset<kSingleDeviceSetBitSize> number = 0b0;
  for (const AudioDevice& device : devices) {
    CountDevice(number, device);
  }
  return static_cast<uint32_t>(number.to_ulong());
}

uint32_t EncodeBeforeAndAfterAudioDeviceSets(
    const AudioDeviceList& device_set_before,
    const AudioDeviceList& device_set_after) {
  uint32_t number_before = EncodeAudioDeviceSet(device_set_before);
  uint32_t number_after = EncodeAudioDeviceSet(device_set_after);
  return (number_before << kSingleDeviceSetBitSize) | number_after;
}

}  // namespace ash
