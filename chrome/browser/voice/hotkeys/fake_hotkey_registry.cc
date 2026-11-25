// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/voice/hotkeys/fake_hotkey_registry.h"

#include "base/memory/raw_ref.h"

namespace voice {

class FakeHotkeyRegistry::Registration
    : public HotkeyRegistrationDelegate::ScopedRegistration {
 public:
  explicit Registration(FakeHotkeyRegistry& registry) : registry_(registry) {}
  Registration(const Registration&) = delete;
  Registration& operator=(const Registration&) = delete;
  ~Registration() override { registry_->OnUnregistered(); }

 private:
  const raw_ref<FakeHotkeyRegistry> registry_;
};

class FakeHotkeyRegistry::Delegate : public HotkeyRegistrationDelegate {
 public:
  explicit Delegate(FakeHotkeyRegistry& registry) : registry_(registry) {}
  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;
  ~Delegate() override = default;

  // HotkeyRegistrationDelegate:
  std::unique_ptr<ScopedRegistration> Register(
      const ui::Accelerator& accelerator,
      ui::AcceleratorTarget& target) override {
    registry_->OnRegistered(accelerator);
    return std::make_unique<Registration>(*registry_);
  }

 private:
  const raw_ref<FakeHotkeyRegistry> registry_;
};

FakeHotkeyRegistry::FakeHotkeyRegistry() = default;

FakeHotkeyRegistry::~FakeHotkeyRegistry() = default;

std::unique_ptr<HotkeyRegistrationDelegate>
FakeHotkeyRegistry::CreateDelegate() {
  return std::make_unique<Delegate>(*this);
}

void FakeHotkeyRegistry::OnRegistered(const ui::Accelerator& accelerator) {
  last_registered_accelerator_ = accelerator;
  ++registration_count_;
}

void FakeHotkeyRegistry::OnUnregistered() {
  ++destruction_count_;
}

}  // namespace voice
