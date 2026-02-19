// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace {

class ScopedCallToActionLockImpl : public ScopedCallToActionLock {
 public:
  explicit ScopedCallToActionLockImpl(
      base::WeakPtr<CallToActionLock> call_to_action)
      : call_to_action_(call_to_action) {}
  ~ScopedCallToActionLockImpl() override {
    if (call_to_action_) {
      call_to_action_->OnScopedCallToActionLockDestroyed();
    }
  }

 private:
  base::WeakPtr<CallToActionLock> call_to_action_;
};

}  // namespace

DEFINE_USER_DATA(CallToActionLock);

CallToActionLock::CallToActionLock(BrowserWindowInterface* browser_window)
    : scoped_user_data_(browser_window->GetUnownedUserDataHost(), *this) {}

CallToActionLock::~CallToActionLock() = default;

// static
CallToActionLock* CallToActionLock::From(
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return nullptr;
  }
  return Get(browser_window->GetUnownedUserDataHost());
}

bool CallToActionLock::CanAcquireLock() const {
  return !showing_call_to_action_;
}

std::unique_ptr<ScopedCallToActionLock> CallToActionLock::AcquireLock() {
  CHECK(!showing_call_to_action_);
  showing_call_to_action_ = true;
  return std::make_unique<ScopedCallToActionLockImpl>(
      weak_factory_.GetWeakPtr());
}

void CallToActionLock::OnScopedCallToActionLockDestroyed() {
  showing_call_to_action_ = false;
}
