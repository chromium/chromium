// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor_delegate.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

bool AccountReconcilorDelegate::IsReconcileEnabled() const {
  return false;
}

bool AccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return false;
}

std::string AccountReconcilorDelegate::GetGaiaApiSource() const {
  NOTREACHED() << "Reconcile is not enabled, no Gaia API calls should be made.";
  return "ChromiumAccountReconcilorInvalidSource";
}

bool AccountReconcilorDelegate::ShouldAbortReconcileIfPrimaryHasError() const {
  return false;
}

std::string AccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution,
    bool will_logout) const {
  return std::string();
}

MultiloginParameters
AccountReconcilorDelegate::CalculateParametersForMultilogin(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error) const {
  const MultiloginMode mode = CalculateModeForReconcile(
      gaia_accounts, primary_account, first_execution, primary_has_error);
  const std::vector<std::string> accounts_to_send =
      ReorderChromeAccountsForReconcile(chrome_accounts, primary_account,
                                        gaia_accounts, mode);
  return {mode, accounts_to_send};
}

MultiloginMode AccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string primary_account,
    bool first_execution,
    bool primary_has_error) const {
  return MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
}

std::vector<std::string>
AccountReconcilorDelegate::ReorderChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const signin::MultiloginMode mode) const {
  return std::vector<std::string>();
}

AccountReconcilorDelegate::RevokeTokenOption
AccountReconcilorDelegate::ShouldRevokeSecondaryTokensBeforeReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  return RevokeTokenOption::kDoNotRevoke;
}

bool AccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted() {
  return false;
}

base::TimeDelta AccountReconcilorDelegate::GetReconcileTimeout() const {
  return base::TimeDelta::Max();
}

void AccountReconcilorDelegate::OnReconcileError(
    const GoogleServiceAuthError& error) {}

}  // namespace signin
