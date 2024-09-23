// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_IMPL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/components/audio/system_sounds_delegate.h"

// Handles initializing and playing ash system sounds when requested.
// TODO(b:365893870): Remove SystemSoundsDelegate and unify the implementation
// with tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) SystemSoundsDelegateImpl
    : public ash::SystemSoundsDelegate {
 public:
  SystemSoundsDelegateImpl();
  SystemSoundsDelegateImpl(const SystemSoundsDelegateImpl&) = delete;
  SystemSoundsDelegateImpl& operator=(const SystemSoundsDelegateImpl&) = delete;
  ~SystemSoundsDelegateImpl() override;

  // ash::SystemSoundsDelegate:
  void Init() override;
  void Play(ash::Sound sound_key) override;
};

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_SYSTEM_SOUNDS_DELEGATE_IMPL_H_
