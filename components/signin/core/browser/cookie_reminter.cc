// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/cookie_reminter.h"

#include "base/callback_helpers.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

namespace {

bool DoesAccountRequireCookieReminting(
    const std::vector<CoreAccountInfo>& accounts_requiring_cookie_remint,
    const CoreAccountInfo& account_info) {
  for (const CoreAccountInfo& account_requiring_remint :
       accounts_requiring_cookie_remint) {
    if (account_info.gaia == account_requiring_remint.gaia) {
      return true;
    }
  }

  return false;
}

}  // namespace

CookieReminter::CookieReminter(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);
}

CookieReminter::~CookieReminter() {
  identity_manager_->RemoveObserver(this);
}

void CookieReminter::ForceCookieRemintingOnNextTokenUpdate(
    const CoreAccountInfo& account_info) {
  if (DoesAccountRequireCookieReminting(accounts_requiring_cookie_remint_,
                                        account_info)) {
    return;
  }

  accounts_requiring_cookie_remint_.emplace_back(account_info);
}

void CookieReminter::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (DoesAccountRequireCookieReminting(accounts_requiring_cookie_remint_,
                                        account_info)) {
    // Cookies are going to be reminted for all accounts.
    accounts_requiring_cookie_remint_.clear();
    identity_manager_->GetAccountsCookieMutator()->LogOutAllAccounts(
        gaia::GaiaSource::kChromeOS, base::DoNothing());
  }
}
