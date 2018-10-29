// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/profile_management_switches.h"

class SigninClient;

namespace signin {

// AccountReconcilorDelegate specialized for Dice.
class DiceAccountReconcilorDelegate : public AccountReconcilorDelegate {
 public:
  DiceAccountReconcilorDelegate(SigninClient* signin_client,
                                AccountConsistencyMethod account_consistency);
  ~DiceAccountReconcilorDelegate() override {}

  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  bool IsAccountConsistencyEnforced() const override;
  std::string GetGaiaApiSource() const override;
  std::string GetFirstGaiaAccountForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string& primary_account,
      bool first_execution,
      bool will_logout) const override;
  RevokeTokenOption ShouldRevokeSecondaryTokensBeforeReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts) override;
  void OnReconcileFinished(const std::string& first_account,
                           bool reconcile_is_noop) override;
  bool ShouldRevokeTokensOnCookieDeleted() override;

 private:
  std::vector<std::string> ReorderChromeAccountsForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::string& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const signin::MultiloginMode mode) const override;

  MultiloginMode CalculateModeForReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string primary_account,
      bool first_execution,
      bool primary_has_error) const override;

  SigninClient* signin_client_;
  AccountConsistencyMethod account_consistency_;

  // Last known "first account". Used when cookies are lost as a best guess.
  std::string last_known_first_account_;

  DISALLOW_COPY_AND_ASSIGN(DiceAccountReconcilorDelegate);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
