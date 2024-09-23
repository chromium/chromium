// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/sounds.h"

namespace ash {

// Defines the interface for the delegate of `SystemSoundsDelegateImpl` and
// `TestSystemSoundsDelegate`. The `Shell` owns the instance of this delegate.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) SystemSoundsDelegate {
 public:
  virtual ~SystemSoundsDelegate() = default;

  // Initializes and bundles system sounds wav data with different sound keys.
  virtual void Init() = 0;

  // Plays sound according to the `sound_key`.
  virtual void Play(Sound sound_key) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_H_
