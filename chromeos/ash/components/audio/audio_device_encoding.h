// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_ENCODING_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_ENCODING_H_

#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

// Encodes a set of audio devices with a 14-bit integer.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
uint32_t EncodeAudioDeviceSet(const AudioDeviceList& devices);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_ENCODING_H_
