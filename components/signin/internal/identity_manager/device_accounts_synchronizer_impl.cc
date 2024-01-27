// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"

#include "base/check.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace signin {

DeviceAccountsSynchronizerImpl::DeviceAccountsSynchronizerImpl(
    ProfileOAuth2TokenServiceDelegate* token_service_delegate)
    : token_service_delegate_(token_service_delegate) {
  CHECK(token_service_delegate_);
}

DeviceAccountsSynchronizerImpl::~DeviceAccountsSynchronizerImpl() = default;

void DeviceAccountsSynchronizerImpl::
    ReloadAllAccountsFromSystemWithPrimaryAccount(
        const std::optional<CoreAccountId>& primary_account_id) {
  token_service_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
      primary_account_id);
}

#if BUILDFLAG(IS_ANDROID)
void DeviceAccountsSynchronizerImpl::
    SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
        const std::vector<CoreAccountInfo>& core_account_infos,
        const std::optional<CoreAccountId>& primary_account_id) {
  token_service_delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      core_account_infos, primary_account_id);
}
#endif

#if BUILDFLAG(IS_IOS)
void DeviceAccountsSynchronizerImpl::ReloadAccountFromSystem(
    const CoreAccountId& account_id) {
  token_service_delegate_->ReloadAccountFromSystem(account_id);
}
#endif

}  // namespace signin
