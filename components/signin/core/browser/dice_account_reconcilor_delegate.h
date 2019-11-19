// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"

class SigninClient;

// Enables usage of Gaia Auth Multilogin endpoint for identity consistency.
extern const base::Feature kUseMultiloginEndpoint;

namespace signin {

// AccountReconcilorDelegate specialized for Dice.
class DiceAccountReconcilorDelegate : public AccountReconcilorDelegate {
 public:
  DiceAccountReconcilorDelegate(SigninClient* signin_client,
                                AccountConsistencyMethod account_consistency,
                                bool migration_completed);
  ~DiceAccountReconcilorDelegate() override {}

  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  bool IsMultiloginEndpointEnabled() const override;
  bool IsAccountConsistencyEnforced() const override;
  void MaybeLogInconsistencyReason(
      const CoreAccountId& primary_account,
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution) const override;
  gaia::GaiaSource GetGaiaApiSource() const override;
  CoreAccountId GetFirstGaiaAccountForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool will_logout) const override;
  RevokeTokenOption ShouldRevokeSecondaryTokensBeforeReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts) override;
  // Returns true if in force migration to dice state.
  bool ShouldRevokeTokensNotInCookies() const override;
  // Disables force dice migration and sets dice migration as completed.
  void OnRevokeTokensNotInCookiesCompleted(
      RevokeTokenAction revoke_token_action) override;
  void OnReconcileFinished(const CoreAccountId& first_account,
                           bool reconcile_is_noop) override;
  bool ShouldRevokeTokensOnCookieDeleted() override;

 private:
  // Possible inconsistency reasons between tokens and gaia cookies.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InconsistencyReason {
    // Consistent
    kNone = 0,
    // Inconsistent
    kMissingSyncCookie = 1,
    kSyncAccountAuthError = 2,
    kMissingFirstWebAccountToken = 3,
    kMissingSecondaryCookie = 4,
    kMissingSecondaryToken = 5,
    kCookieTokenMismatch = 6,
    kSyncCookieNotFirst = 7,
    kMaxValue = kSyncCookieNotFirst
  };

  // Computes inconsistency reason between tokens and gaia cookies.
  InconsistencyReason GetInconsistencyReason(
      const CoreAccountId& primary_account,
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution) const;

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

  SigninClient* signin_client_;
  AccountConsistencyMethod account_consistency_;
  bool migration_completed_;

  // Last known "first account". Used when cookies are lost as a best guess.
  CoreAccountId last_known_first_account_;

  DISALLOW_COPY_AND_ASSIGN(DiceAccountReconcilorDelegate);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
