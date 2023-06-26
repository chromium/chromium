// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/fake_lock_handler.h"

namespace proximity_auth {

FakeLockHandler::FakeLockHandler() {}

FakeLockHandler::~FakeLockHandler() {}

void FakeLockHandler::ShowBannerMessage(const std::u16string& message,
                                        bool is_warning) {}

void FakeLockHandler::SetSmartLockState(const AccountId& account_id,
                                        ash::SmartLockState state) {
  smart_lock_state_ = state;
}

void FakeLockHandler::NotifySmartLockAuthResult(const AccountId& account_id,
                                                bool successful) {
  smart_lock_auth_result_ = successful;
}

void FakeLockHandler::EnableInput() {}

void FakeLockHandler::SetAuthType(const AccountId& account_id,
                                  mojom::AuthType auth_type,
                                  const std::u16string& auth_value) {
  latest_set_auth_type_ = auth_type;
}

mojom::AuthType FakeLockHandler::GetAuthType(
    const AccountId& account_id) const {
  return latest_set_auth_type_;
}

FakeLockHandler::ScreenType FakeLockHandler::GetScreenType() const {
  return FakeLockHandler::LOCK_SCREEN;
}

void FakeLockHandler::Unlock(const AccountId& account_id) {
  unlock_called_++;
}

void FakeLockHandler::ClearSmartLockState() {
  smart_lock_state_.reset();
}

void FakeLockHandler::ClearSmartLockAuthResult() {
  smart_lock_auth_result_.reset();
}

}  // namespace proximity_auth
