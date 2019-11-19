// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_

#include "build/build_config.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"

class ProfileOAuth2TokenServiceDelegate;

namespace signin {

// Concrete implementation of DeviceAccountsSynchronizer interface.
class DeviceAccountsSynchronizerImpl : public DeviceAccountsSynchronizer {
 public:
  explicit DeviceAccountsSynchronizerImpl(
      ProfileOAuth2TokenServiceDelegate* token_service_delegate);
  ~DeviceAccountsSynchronizerImpl() override;

  // DeviceAccountsSynchronizer implementation.
#if defined(OS_ANDROID)
  void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const base::Optional<CoreAccountId>& primary_account_id) override;
#endif

#if defined(OS_IOS)
  void ReloadAllAccountsFromSystem() override;
  void ReloadAccountFromSystem(const CoreAccountId& account_id) override;
#endif

 private:
  ProfileOAuth2TokenServiceDelegate* token_service_delegate_ = nullptr;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
