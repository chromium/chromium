// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"

#include "base/optional.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    PrimaryAccountManager* primary_account_manager,
    PrefService* pref_service)
    : token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      primary_account_manager_(primary_account_manager) {
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
  DCHECK(primary_account_manager_);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  pref_service_ = pref_service;
  DCHECK(pref_service_);
#endif
}

AccountsMutatorImpl::~AccountsMutatorImpl() {}

CoreAccountId AccountsMutatorImpl::AddOrUpdateAccount(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& refresh_token,
    bool is_under_advanced_protection,
    signin_metrics::SourceForRefreshTokenOperation source) {
  CoreAccountId account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      account_id, is_under_advanced_protection);
  token_service_->UpdateCredentials(account_id, refresh_token, source);

  return account_id;
}

void AccountsMutatorImpl::UpdateAccountInfo(
    const CoreAccountId& account_id,
    base::Optional<bool> is_child_account,
    base::Optional<bool> is_under_advanced_protection) {
  if (is_child_account.has_value()) {
    account_tracker_service_->SetIsChildAccount(account_id,
                                                is_child_account.value());
  }

  if (is_under_advanced_protection.has_value()) {
    account_tracker_service_->SetIsAdvancedProtectionAccount(
        account_id, is_under_advanced_protection.value());
  }
}

void AccountsMutatorImpl::RemoveAccount(
    const CoreAccountId& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeCredentials(account_id, source);
}

void AccountsMutatorImpl::RemoveAllAccounts(
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeAllCredentials(source);
}

void AccountsMutatorImpl::InvalidateRefreshTokenForPrimaryAccount(
    signin_metrics::SourceForRefreshTokenOperation source) {
  DCHECK(primary_account_manager_->IsAuthenticated());
  CoreAccountInfo primary_account_info =
      primary_account_manager_->GetAuthenticatedAccountInfo();
  AddOrUpdateAccount(primary_account_info.gaia, primary_account_info.email,
                     GaiaConstants::kInvalidRefreshToken,
                     primary_account_info.is_under_advanced_protection, source);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountsMutatorImpl::MoveAccount(AccountsMutator* target,
                                      const CoreAccountId& account_id) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.account_id.empty());

  auto* target_impl = static_cast<AccountsMutatorImpl*>(target);
  target_impl->account_tracker_service_->SeedAccountInfo(account_info);
  token_service_->ExtractCredentials(target_impl->token_service_, account_id);

  // Reset the device ID from the source mutator: the exported token is linked
  // to the device ID of the current mutator on the server. Reset the device ID
  // of the current mutator to avoid tying it with the new mutator. See
  // https://crbug.com/813928#c16
  RecreateSigninScopedDeviceId(pref_service_);
}
#endif

}  // namespace signin
