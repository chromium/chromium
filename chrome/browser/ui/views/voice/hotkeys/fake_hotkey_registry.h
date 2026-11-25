// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_FAKE_HOTKEY_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_FAKE_HOTKEY_REGISTRY_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/views/voice/hotkeys/hotkey_registration_delegate.h"
#include "ui/base/accelerators/accelerator.h"

namespace voice {

// Records the hotkeys registered through the delegates it creates, instead of
// registering them with any window. Must outlive the delegates and
// registrations it creates.
class FakeHotkeyRegistry {
 public:
  FakeHotkeyRegistry();
  FakeHotkeyRegistry(const FakeHotkeyRegistry&) = delete;
  FakeHotkeyRegistry& operator=(const FakeHotkeyRegistry&) = delete;
  ~FakeHotkeyRegistry();

  // Returns a delegate whose registrations are recorded by `this`.
  std::unique_ptr<HotkeyRegistrationDelegate> CreateDelegate();

  // Whether any hotkey is currently registered.
  bool is_registered() const {
    return registration_count_ > destruction_count_;
  }

  // The most recently registered accelerator, if any.
  const std::optional<ui::Accelerator>& last_registered_accelerator() const {
    return last_registered_accelerator_;
  }

  // The number of registrations created and destroyed so far.
  int registration_count() const { return registration_count_; }
  int destruction_count() const { return destruction_count_; }

 private:
  class Delegate;
  class Registration;

  void OnRegistered(const ui::Accelerator& accelerator);
  void OnUnregistered();

  std::optional<ui::Accelerator> last_registered_accelerator_;
  int registration_count_ = 0;
  int destruction_count_ = 0;
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_FAKE_HOTKEY_REGISTRY_H_
