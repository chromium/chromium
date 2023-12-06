// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {

class FakeLockHandler : public ScreenlockBridge::LockHandler {
 public:
  FakeLockHandler();

  FakeLockHandler(const FakeLockHandler&) = delete;
  FakeLockHandler& operator=(const FakeLockHandler&) = delete;

  ~FakeLockHandler() override;

  // LockHandler:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override;
  void SetSmartLockState(const AccountId& account_id,
                         ash::SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool successful) override;
  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   mojom::AuthType auth_type,
                   const std::u16string& auth_value) override;
  mojom::AuthType GetAuthType(const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;
  void Unlock(const AccountId& account_id) override;

  std::optional<ash::SmartLockState> smart_lock_state() const {
    return smart_lock_state_;
  }
  std::optional<bool> smart_lock_auth_result() const {
    return smart_lock_auth_result_;
  }
  int unlock_called() const { return unlock_called_; }

  void ClearSmartLockState();
  void ClearSmartLockAuthResult();

 private:
  std::optional<ash::SmartLockState> smart_lock_state_;
  std::optional<bool> smart_lock_auth_result_;
  mojom::AuthType latest_set_auth_type_ = mojom::AuthType::USER_CLICK;
  int unlock_called_ = 0;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
