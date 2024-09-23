// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"

namespace signin {

class IdentityManager;

// AccountReconcilorDelegate specialized for Dice.
class DiceAccountReconcilorDelegate : public AccountReconcilorDelegate {
 public:
  DiceAccountReconcilorDelegate(IdentityManager* identity_manager,
                                SigninClient* signin_client);

  DiceAccountReconcilorDelegate(const DiceAccountReconcilorDelegate&) = delete;
  DiceAccountReconcilorDelegate& operator=(
      const DiceAccountReconcilorDelegate&) = delete;

  ~DiceAccountReconcilorDelegate() override;

  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  gaia::GaiaSource GetGaiaApiSource() const override;
  void RevokeSecondaryTokensForReconcileIfNeeded(
      const std::vector<gaia::ListedAccount>& gaia_accounts) override;
  void OnReconcileFinished(const CoreAccountId& first_account) override;
  void OnAccountsCookieDeletedByUserAction(
      bool synced_data_deletion_in_progress) override;
  bool RevokeSecondaryTokensBeforeMultiloginIfNeeded(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution) override;
  ConsentLevel GetConsentLevelForPrimaryAccount() const override;

  // Returns true if explicit browser sign in is enabled and Chrome isn't signed
  // in.
  // In this mode:
  // - First, refresh tokens that do not have a valid counter account in the
  //   cookie are revoked.
  // - Then if needed, the cookie is updated to remove accounts that do not
  //   have a refresh token. This is possible:
  //   (1) If the user has signed out from chrome while being offline.
  //   (2) If an account is moved from a profile to another as part of the
  //       Sign in interception flows or as a result of merge sync data flow.
  // Public for testing.
  bool IsCookieBasedConsistencyMode() const;

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

  // Used when update cookies to match refresh tokens is not allowed, see
  // `IsUpdateCookieAllowed()`. In this mode, refresh tokens are updated to
  // match accounts in the Gaia cookies to maintain consistency.
  void MatchTokensWithAccountsInCookie(
      const std::vector<gaia::ListedAccount>& gaia_accounts);

  // Computes inconsistency reason between tokens and gaia cookies.
  InconsistencyReason GetInconsistencyReason(
      const CoreAccountId& primary_account,
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution) const;

  // AccountReconcilorDelegate:
  std::vector<CoreAccountId> GetChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error,
      const gaia::MultiloginMode mode) const override;
  gaia::MultiloginMode CalculateModeForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool primary_has_error) const override;

  // Checks if Preserve mode is possible. Preserve mode fails if there is a
  // valid cookie and no matching valid token. If first_account is not empty,
  // then this account must be first in the cookie after the Preserve mode is
  // performed.
  bool IsPreserveModePossible(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      CoreAccountId first_account) const;

  // Checks if there are valid cookies that should be deleted. That's happening
  // if there is a valid cookie that doesn't have a valid token.
  bool ShouldDeleteAccountsFromGaia(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts) const;

  // Returns the first account to add in the Gaia cookie for multilogin.
  // If this returns an empty account, it means any account can come first.
  // The order for other accounts will be selected outside of this function
  // using ReorderChromeAccountsForReconcile function to minimize account
  // re-numbering.
  CoreAccountId GetFirstGaiaAccountForMultilogin(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error) const;

  // Returns whether secondary accounts should be revoked for doing full logout.
  // Used only for the Multilogin codepath.
  bool ShouldRevokeTokensBeforeMultilogin(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution) const;

  const raw_ptr<IdentityManager> identity_manager_;
  const raw_ptr<SigninClient> signin_client_;

  // Last known "first account". Used when cookies are lost as a best guess.
  CoreAccountId last_known_first_account_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_ACCOUNT_RECONCILOR_DELEGATE_H_
