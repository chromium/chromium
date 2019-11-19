// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"

#include "base/logging.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

namespace signin {

DeviceAccountsSynchronizerImpl::DeviceAccountsSynchronizerImpl(
    ProfileOAuth2TokenServiceDelegate* token_service_delegate)
    : token_service_delegate_(token_service_delegate) {
  DCHECK(token_service_delegate_);
}

DeviceAccountsSynchronizerImpl::~DeviceAccountsSynchronizerImpl() = default;

#if defined(OS_ANDROID)
void DeviceAccountsSynchronizerImpl::
    ReloadAllAccountsFromSystemWithPrimaryAccount(
        const base::Optional<CoreAccountId>& primary_account_id) {
  token_service_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
      primary_account_id);
}
#endif

#if defined(OS_IOS)
void DeviceAccountsSynchronizerImpl::ReloadAllAccountsFromSystem() {
  token_service_delegate_->ReloadAllAccountsFromSystem();
}

void DeviceAccountsSynchronizerImpl::ReloadAccountFromSystem(
    const CoreAccountId& account_id) {
  token_service_delegate_->ReloadAccountFromSystem(account_id);
}
#endif

}  // namespace signin
