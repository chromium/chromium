// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_SOUNDS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_SOUNDS_DELEGATE_IMPL_H_

#include "ash/public/cpp/system_sounds_delegate.h"
#include "chromeos/ash/components/audio/sounds.h"

// Handles initializing and playing ash system sounds when requested.
class SystemSoundsDelegateImpl : public ash::SystemSoundsDelegate {
 public:
  SystemSoundsDelegateImpl();
  SystemSoundsDelegateImpl(const SystemSoundsDelegateImpl&) = delete;
  SystemSoundsDelegateImpl& operator=(const SystemSoundsDelegateImpl&) = delete;
  ~SystemSoundsDelegateImpl() override;

  // ash::SystemSoundsDelegate:
  void Init() override;
  void Play(ash::Sound sound_key) override;
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_SOUNDS_DELEGATE_IMPL_H_
