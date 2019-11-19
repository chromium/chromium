// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_MICE_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_MICE_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"

namespace signin {

// AccountReconcilorDelegate specialized for Mice.
class MiceAccountReconcilorDelegate : public AccountReconcilorDelegate {
 public:
  MiceAccountReconcilorDelegate();
  ~MiceAccountReconcilorDelegate() override;

 private:
  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  bool IsAccountConsistencyEnforced() const override;
  gaia::GaiaSource GetGaiaApiSource() const override;
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
  gaia::MultiloginMode CalculateModeForReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool primary_has_error) const override;
  bool IsUnknownInvalidAccountInCookieAllowed() const override;

  DISALLOW_COPY_AND_ASSIGN(MiceAccountReconcilorDelegate);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_MICE_ACCOUNT_RECONCILOR_DELEGATE_H_
