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

bool EnumTraits<arc::mojom::AudioDeviceType, ash::AudioDeviceType>::FromMojom(
    arc::mojom::AudioDeviceType input,
    ash::AudioDeviceType* out) {
  switch (input) {
    case arc::mojom::AudioDeviceType::HEADPHONE:
      *out = ash::AudioDeviceType::kHeadphone;
      return true;
    case arc::mojom::AudioDeviceType::MIC:
      *out = ash::AudioDeviceType::kMic;
      return true;
    case arc::mojom::AudioDeviceType::USB:
      *out = ash::AudioDeviceType::kUsb;
      return true;
    case arc::mojom::AudioDeviceType::BLUETOOTH:
      *out = ash::AudioDeviceType::kBluetooth;
      return true;
    case arc::mojom::AudioDeviceType::BLUETOOTH_NB_MIC:
      *out = ash::AudioDeviceType::kBluetoothNbMic;
      return true;
    case arc::mojom::AudioDeviceType::HDMI:
      *out = ash::AudioDeviceType::kHdmi;
      return true;
    case arc::mojom::AudioDeviceType::INTERNAL_SPEAKER:
      *out = ash::AudioDeviceType::kInternalSpeaker;
      return true;
    case arc::mojom::AudioDeviceType::INTERNAL_MIC:
      *out = ash::AudioDeviceType::kInternalMic;
      return true;
    case arc::mojom::AudioDeviceType::FRONT_MIC:
      *out = ash::AudioDeviceType::kFrontMic;
      return true;
    case arc::mojom::AudioDeviceType::REAR_MIC:
      *out = ash::AudioDeviceType::kRearMic;
      return true;
    case arc::mojom::AudioDeviceType::KEYBOARD_MIC:
      *out = ash::AudioDeviceType::kKeyboardMic;
      return true;
    case arc::mojom::AudioDeviceType::HOTWORD:
      *out = ash::AudioDeviceType::kHotword;
      return true;
    case arc::mojom::AudioDeviceType::LINEOUT:
      *out = ash::AudioDeviceType::kLineout;
      return true;
    case arc::mojom::AudioDeviceType::POST_MIX_LOOPBACK:
      *out = ash::AudioDeviceType::kPostMixLoopback;
      return true;
    case arc::mojom::AudioDeviceType::POST_DSP_LOOPBACK:
      *out = ash::AudioDeviceType::kPostDspLoopback;
      return true;
    case arc::mojom::AudioDeviceType::ALSA_LOOPBACK:
      *out = ash::AudioDeviceType::kAlsaLoopback;
      return true;
    case arc::mojom::AudioDeviceType::OTHER:
      *out = ash::AudioDeviceType::kOther;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
