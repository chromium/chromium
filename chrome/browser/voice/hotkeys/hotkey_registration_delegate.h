// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_REGISTRATION_DELEGATE_H_
#define CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_REGISTRATION_DELEGATE_H_

#include <memory>

namespace ui {
class Accelerator;
class AcceleratorTarget;
}  // namespace ui

namespace voice {

// Registers hotkeys within some scope, e.g. all browser windows of a profile.
// Allows for different registration mechanisms, such as fakes in tests.
class HotkeyRegistrationDelegate {
 public:
  // Keeps a hotkey registered for as long as it is alive.
  class ScopedRegistration {
   public:
    virtual ~ScopedRegistration() = default;
  };

  virtual ~HotkeyRegistrationDelegate() = default;

  // Registers `accelerator` so that pressing it calls
  // `target.AcceleratorPressed()`, until the returned object is destroyed.
  // `target` must outlive the returned object.
  virtual std::unique_ptr<ScopedRegistration> Register(
      const ui::Accelerator& accelerator,
      ui::AcceleratorTarget& target) = 0;
};

}  // namespace voice

#endif  // CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_REGISTRATION_DELEGATE_H_
