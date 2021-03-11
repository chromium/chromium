// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_

#include "base/macros.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {

class FakeLockHandler : public ScreenlockBridge::LockHandler {
 public:
  FakeLockHandler();
  ~FakeLockHandler() override;

  // LockHandler:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const ScreenlockBridge::UserPodCustomIconOptions& icon) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   mojom::AuthType auth_type,
                   const std::u16string& auth_value) override;
  mojom::AuthType GetAuthType(const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;
  void Unlock(const AccountId& account_id) override;
  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLockHandler);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
