// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VOICE_HOTKEYS_APPLICATION_HOTKEY_REGISTRATION_DELEGATE_H_
#define CHROME_BROWSER_VOICE_HOTKEYS_APPLICATION_HOTKEY_REGISTRATION_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/voice/hotkeys/hotkey_registration_delegate.h"

class Profile;

namespace voice {

// A registration delegate that registers hotkeys application-wide for a
// specific profile, i.e. with the FocusManager of all of its current and
// future browser windows.
class ApplicationHotkeyRegistrationDelegate
    : public HotkeyRegistrationDelegate {
 public:
  // `profile` must outlive `this` and the registrations it creates.
  explicit ApplicationHotkeyRegistrationDelegate(Profile& profile);
  ApplicationHotkeyRegistrationDelegate(
      const ApplicationHotkeyRegistrationDelegate&) = delete;
  ApplicationHotkeyRegistrationDelegate& operator=(
      const ApplicationHotkeyRegistrationDelegate&) = delete;
  ~ApplicationHotkeyRegistrationDelegate() override;

  // HotkeyRegistrationDelegate:
  std::unique_ptr<ScopedRegistration> Register(
      const ui::Accelerator& accelerator,
      ui::AcceleratorTarget& target) override;

 private:
  const raw_ref<Profile> profile_;
};

}  // namespace voice

#endif  // CHROME_BROWSER_VOICE_HOTKEYS_APPLICATION_HOTKEY_REGISTRATION_DELEGATE_H_
