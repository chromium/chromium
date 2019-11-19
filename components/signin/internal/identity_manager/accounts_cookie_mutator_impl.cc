// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"

#include <utility>

#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

AccountsCookieMutatorImpl::AccountsCookieMutatorImpl(
    GaiaCookieManagerService* gaia_cookie_manager_service,
    AccountTrackerService* account_tracker_service)
    : gaia_cookie_manager_service_(gaia_cookie_manager_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(gaia_cookie_manager_service_);
  DCHECK(account_tracker_service_);
}

AccountsCookieMutatorImpl::~AccountsCookieMutatorImpl() {}

void AccountsCookieMutatorImpl::AddAccountToCookie(
    const CoreAccountId& account_id,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookie(
      account_id, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::AddAccountToCookieWithToken(
    const CoreAccountId& account_id,
    const std::string& access_token,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookieWithToken(
      account_id, access_token, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::SetAccountsInCookie(
    const MultiloginParameters& parameters,
    gaia::GaiaSource source,
    base::OnceCallback<void(SetAccountsInCookieResult)>
        set_accounts_in_cookies_completed_callback) {
  std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair> accounts;
  for (const auto& account_id : parameters.accounts_to_send) {
    accounts.push_back(make_pair(
        account_id, account_tracker_service_->GetAccountInfo(account_id).gaia));
  }
  gaia_cookie_manager_service_->SetAccountsInCookie(
      parameters.mode, accounts, source,
      std::move(set_accounts_in_cookies_completed_callback));
}

void AccountsCookieMutatorImpl::TriggerCookieJarUpdate() {
  gaia_cookie_manager_service_->TriggerListAccounts();
}

#if defined(OS_IOS)
void AccountsCookieMutatorImpl::ForceTriggerOnCookieChange() {
  gaia_cookie_manager_service_->ForceOnCookieChangeProcessing();
}
#endif

void AccountsCookieMutatorImpl::LogOutAllAccounts(gaia::GaiaSource source) {
  gaia_cookie_manager_service_->LogOutAllAccounts(source);
}

}  // namespace signin
