// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_DIRECTORY_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_DIRECTORY_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"

namespace signin {

// AccountReconcilorDelegate specialized for Active Directory accounts.
class ActiveDirectoryAccountReconcilorDelegate
    : public AccountReconcilorDelegate {
 public:
  ActiveDirectoryAccountReconcilorDelegate();
  ~ActiveDirectoryAccountReconcilorDelegate() override;

  // AccountReconcilorDelegate:
  bool IsAccountConsistencyEnforced() const override;
  gaia::GaiaSource GetGaiaApiSource() const override;
  bool IsReconcileEnabled() const override;
  CoreAccountId GetFirstGaiaAccountForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool will_logout) const override;

  std::vector<CoreAccountId> GetChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const gaia::MultiloginMode mode) const override;
  bool ShouldAbortReconcileIfPrimaryHasError() const override;
  bool ShouldRevokeTokensIfNoPrimaryAccount() const override;

 private:
  // Returns first gaia account if it is present in |chrome_accounts|.
  // Otherwise, the first chrome account is returned.
  CoreAccountId GetFirstAccount(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts) const;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryAccountReconcilorDelegate);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_DIRECTORY_ACCOUNT_RECONCILOR_DELEGATE_H_
