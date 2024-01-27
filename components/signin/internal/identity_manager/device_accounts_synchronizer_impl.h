// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_

#include "base/memory/raw_ptr.h"
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
  void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const std::optional<CoreAccountId>& primary_account_id) override;

#if BUILDFLAG(IS_ANDROID)
  void SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      const std::vector<CoreAccountInfo>& core_account_infos,
      const std::optional<CoreAccountId>& primary_account_id) override;
#endif

#if BUILDFLAG(IS_IOS)
  void ReloadAccountFromSystem(const CoreAccountId& account_id) override;
#endif

 private:
  raw_ptr<ProfileOAuth2TokenServiceDelegate> token_service_delegate_ = nullptr;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
