// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/audio/audio_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

arc::mojom::AudioDeviceType
EnumTraits<arc::mojom::AudioDeviceType, ash::AudioDeviceType>::ToMojom(
    ash::AudioDeviceType audio_device_type) {
  switch (audio_device_type) {
    case ash::AudioDeviceType::kHeadphone:
      return arc::mojom::AudioDeviceType::HEADPHONE;
    case ash::AudioDeviceType::kMic:
      return arc::mojom::AudioDeviceType::MIC;
    case ash::AudioDeviceType::kUsb:
      return arc::mojom::AudioDeviceType::USB;
    case ash::AudioDeviceType::kBluetooth:
      return arc::mojom::AudioDeviceType::BLUETOOTH;
    case ash::AudioDeviceType::kBluetoothNbMic:
      return arc::mojom::AudioDeviceType::BLUETOOTH_NB_MIC;
    case ash::AudioDeviceType::kHdmi:
      return arc::mojom::AudioDeviceType::HDMI;
    case ash::AudioDeviceType::kInternalSpeaker:
      return arc::mojom::AudioDeviceType::INTERNAL_SPEAKER;
    case ash::AudioDeviceType::kInternalMic:
      return arc::mojom::AudioDeviceType::INTERNAL_MIC;
    case ash::AudioDeviceType::kFrontMic:
      return arc::mojom::AudioDeviceType::FRONT_MIC;
    case ash::AudioDeviceType::kRearMic:
      return arc::mojom::AudioDeviceType::REAR_MIC;
    case ash::AudioDeviceType::kKeyboardMic:
      return arc::mojom::AudioDeviceType::KEYBOARD_MIC;
    case ash::AudioDeviceType::kHotword:
      return arc::mojom::AudioDeviceType::HOTWORD;
    case ash::AudioDeviceType::kLineout:
      return arc::mojom::AudioDeviceType::LINEOUT;
    case ash::AudioDeviceType::kPostMixLoopback:
      return arc::mojom::AudioDeviceType::POST_MIX_LOOPBACK;
    case ash::AudioDeviceType::kPostDspLoopback:
      return arc::mojom::AudioDeviceType::POST_DSP_LOOPBACK;
    case ash::AudioDeviceType::kAlsaLoopback:
      return arc::mojom::AudioDeviceType::ALSA_LOOPBACK;
    case ash::AudioDeviceType::kOther:
      return arc::mojom::AudioDeviceType::OTHER;
  }
  NOTREACHED();
}

ash::AudioDeviceType
EnumTraits<arc::mojom::AudioDeviceType, ash::AudioDeviceType>::FromMojom(
    arc::mojom::AudioDeviceType input) {
  switch (input) {
    case arc::mojom::AudioDeviceType::HEADPHONE:
      return ash::AudioDeviceType::kHeadphone;
    case arc::mojom::AudioDeviceType::MIC:
      return ash::AudioDeviceType::kMic;
    case arc::mojom::AudioDeviceType::USB:
      return ash::AudioDeviceType::kUsb;
    case arc::mojom::AudioDeviceType::BLUETOOTH:
      return ash::AudioDeviceType::kBluetooth;
    case arc::mojom::AudioDeviceType::BLUETOOTH_NB_MIC:
      return ash::AudioDeviceType::kBluetoothNbMic;
    case arc::mojom::AudioDeviceType::HDMI:
      return ash::AudioDeviceType::kHdmi;
    case arc::mojom::AudioDeviceType::INTERNAL_SPEAKER:
      return ash::AudioDeviceType::kInternalSpeaker;
    case arc::mojom::AudioDeviceType::INTERNAL_MIC:
      return ash::AudioDeviceType::kInternalMic;
    case arc::mojom::AudioDeviceType::FRONT_MIC:
      return ash::AudioDeviceType::kFrontMic;
    case arc::mojom::AudioDeviceType::REAR_MIC:
      return ash::AudioDeviceType::kRearMic;
    case arc::mojom::AudioDeviceType::KEYBOARD_MIC:
      return ash::AudioDeviceType::kKeyboardMic;
    case arc::mojom::AudioDeviceType::HOTWORD:
      return ash::AudioDeviceType::kHotword;
    case arc::mojom::AudioDeviceType::LINEOUT:
      return ash::AudioDeviceType::kLineout;
    case arc::mojom::AudioDeviceType::POST_MIX_LOOPBACK:
      return ash::AudioDeviceType::kPostMixLoopback;
    case arc::mojom::AudioDeviceType::POST_DSP_LOOPBACK:
      return ash::AudioDeviceType::kPostDspLoopback;
    case arc::mojom::AudioDeviceType::ALSA_LOOPBACK:
      return ash::AudioDeviceType::kAlsaLoopback;
    case arc::mojom::AudioDeviceType::OTHER:
      return ash::AudioDeviceType::kOther;
  }
  NOTREACHED();
}

}  // namespace mojo
