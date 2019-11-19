// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/active_directory_account_reconcilor_delegate.h"

#include "chromeos/tpm/install_attributes.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {

ActiveDirectoryAccountReconcilorDelegate::
    ActiveDirectoryAccountReconcilorDelegate() {
  DCHECK(chromeos::InstallAttributes::Get()->IsActiveDirectoryManaged());
}

ActiveDirectoryAccountReconcilorDelegate::
    ~ActiveDirectoryAccountReconcilorDelegate() = default;

bool ActiveDirectoryAccountReconcilorDelegate::IsAccountConsistencyEnforced()
    const {
  return true;
}

gaia::GaiaSource ActiveDirectoryAccountReconcilorDelegate::GetGaiaApiSource()
    const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
}

bool ActiveDirectoryAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

CoreAccountId
ActiveDirectoryAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool will_logout) const {
  return GetFirstAccount(chrome_accounts, gaia_accounts);
}

std::vector<CoreAccountId>
ActiveDirectoryAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const gaia::MultiloginMode mode) const {
  DCHECK_EQ(mode,
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  if (chrome_accounts.empty())
    return chrome_accounts;
  return ReorderChromeAccountsForReconcile(
      chrome_accounts, GetFirstAccount(chrome_accounts, gaia_accounts),
      gaia_accounts);
}

bool ActiveDirectoryAccountReconcilorDelegate::
    ShouldAbortReconcileIfPrimaryHasError() const {
  return false;
}

bool ActiveDirectoryAccountReconcilorDelegate::
    ShouldRevokeTokensIfNoPrimaryAccount() const {
  return false;
}

CoreAccountId ActiveDirectoryAccountReconcilorDelegate::GetFirstAccount(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts) const {
  if (chrome_accounts.empty())
    return CoreAccountId();
  // Return first gaia account to preserve the account order in the cookie jar.
  // (Gaia accounts which are NOT in chrome_accounts will be removed.) In case
  // of first account mismatch, the cookie will be rebuilt and order of accounts
  // will be changed.
  if (!gaia_accounts.empty() &&
      base::Contains(chrome_accounts, gaia_accounts[0].id)) {
    return gaia_accounts[0].id;
  }
  // The cookie jar is empty or first Gaia account in the cookie jar is
  // not present in Account Manager. Fall back to choosing the first account
  // present in Account Manager as the first account for reconciliation.
  return chrome_accounts[0];
}

}  // namespace signin
