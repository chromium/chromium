// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_AUDIO_AUDIO_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_AUDIO_AUDIO_MOJOM_TRAITS_H_

#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/experiences/arc/mojom/audio.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::AudioDeviceType, ash::AudioDeviceType> {
  static arc::mojom::AudioDeviceType ToMojom(
      ash::AudioDeviceType audio_device_type);
  static bool FromMojom(arc::mojom::AudioDeviceType input,
                        ash::AudioDeviceType* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_AUDIO_AUDIO_MOJOM_TRAITS_H_
