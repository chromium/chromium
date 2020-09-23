// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountReconcilor;

namespace signin {

// Possible revoke token actions taken by the AccountReconcilor.
enum class RevokeTokenAction {
  kNone,
  kInvalidatePrimaryAccountToken,
  kRevokeSecondaryAccountsTokens,
  kRevokeTokensForPrimaryAndSecondaryAccounts,
  kMaxValue = kRevokeTokensForPrimaryAndSecondaryAccounts
};

// Base class for AccountReconcilorDelegate.
class AccountReconcilorDelegate {
 public:
  // Options for revoking refresh tokens.
  enum class RevokeTokenOption {
    // Do not revoke the token.
    kDoNotRevoke,
    // Revoke the token if it is in auth error state.
    kRevokeIfInError,
    // Revoke the token.
    // TODO(droger): remove this when Dice is launched.
    kRevoke
  };

  virtual ~AccountReconcilorDelegate() {}

  // Returns true if the reconcilor should reconcile the profile. Defaults to
  // false.
  virtual bool IsReconcileEnabled() const;

  // Returns whether the OAuth multilogin endpoint can be used to build the Gaia
  // cookies.
  // Default implementation returns true.
  virtual bool IsMultiloginEndpointEnabled() const;

  // Returns true if account consistency is enforced (Mirror or Dice).
  // If this is false, reconcile is done, but its results are discarded and no
  // changes to the accounts are made. Defaults to false.
  virtual bool IsAccountConsistencyEnforced() const;

  // Returns the value to set in the "source" parameter for Gaia API calls.
  virtual gaia::GaiaSource GetGaiaApiSource() const;

  // Returns true if Reconcile should be aborted when the primary account is in
  // error state. Defaults to false.
  virtual bool ShouldAbortReconcileIfPrimaryHasError() const;

  // Returns the consent level that should be used for obtaining the primary
  // account. Defaults to ConsentLevel::kSync.
  virtual ConsentLevel GetConsentLevelForPrimaryAccount() const;

  // Returns the first account to add in the Gaia cookie.
  // If this returns an empty string, the user must be logged out of all
  // accounts.
  // |first_execution| is true for the first reconciliation after startup.
  // |will_logout| is true if the reconcilor will perform a logout no matter
  // what is returned by this function.
  // Only used with MergeSession.
  virtual CoreAccountId GetFirstGaiaAccountForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool will_logout) const;

  // Returns a pair of mode and accounts to send to Mutilogin endpoint.
  MultiloginParameters CalculateParametersForMultilogin(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error) const;

  // Returns whether secondary accounts should be revoked for doing full logout.
  // Used only for the Multilogin codepath.
  virtual bool ShouldRevokeTokensBeforeMultilogin(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error) const;

  // Returns whether secondary accounts should be revoked at the beginning of
  // the reconcile.
  virtual RevokeTokenOption ShouldRevokeSecondaryTokensBeforeReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts);

  // Invalidates primary account token or revokes token for any secondary
  // account that does not have an equivalent gaia cookie.
  virtual bool ShouldRevokeTokensNotInCookies() const;

  // Called when |RevokeTokensNotInCookies| is finished.
  virtual void OnRevokeTokensNotInCookiesCompleted(
      RevokeTokenAction revoke_token_action) {}

  // Returns whether tokens should be revoked when the Gaia cookie has been
  // explicitly deleted by the user.
  // If this returns false, tokens will not be revoked. If this returns true,
  // secondary tokens will be deleted ; and the primary token will be
  // invalidated unless it has to be kept for critical Sync operations.
  virtual bool ShouldRevokeTokensOnCookieDeleted();

  // Returns whether tokens should be revoked when the primary account is empty
  virtual bool ShouldRevokeTokensIfNoPrimaryAccount() const;

  // Called when reconcile is finished.
  // |OnReconcileFinished| is always called at the end of reconciliation,
  // even when there is an error (except in cases where reconciliation times
  // out before finishing, see |GetReconcileTimeout|).
  virtual void OnReconcileFinished(const CoreAccountId& first_account) {}

  // Returns the desired timeout for account reconciliation. If reconciliation
  // does not happen within this time, it is aborted and |this| delegate is
  // informed via |OnReconcileError|, with the 'most severe' error that occurred
  // during this time (see |AccountReconcilor::error_during_last_reconcile_|).
  // If a delegate does not wish to set a timeout for account reconciliation, it
  // should not override this method. Default: |base::TimeDelta::Max()|.
  virtual base::TimeDelta GetReconcileTimeout() const;

  // Called when account reconciliation ends in an error.
  // |OnReconcileError| is called before |OnReconcileFinished|.
  virtual void OnReconcileError(const GoogleServiceAuthError& error);

  // If this returns false, the reconcilor ensures that all accounts unknown to
  // Chrome are always removed from the cookies (even if their session is
  // expired). Returning false is only supported in with multilogin UPDATE mode.
  // Defaults to true.
  virtual bool IsUnknownInvalidAccountInCookieAllowed() const;

  void set_reconcilor(AccountReconcilor* reconcilor) {
    reconcilor_ = reconcilor;
  }
  AccountReconcilor* reconcilor() { return reconcilor_; }

 protected:
  // Computes a new ordering for chrome_accounts.
  // The returned order has the following properties:
  // - first_account will be first if it's not empty.
  // - if a chrome account is also in gaia_accounts, the function tries to keep
  //   it at the same index. The function minimizes account re-numbering.
  // - if there are too many accounts, some accounts will be discarded.
  //   |first_account| and accounts already in cookies will be kept in priority.
  //   Aplhabetical order is used to break ties.
  // Note: the input order of the accounts in |chrome_accounts| does not matter
  // (different orders yield to the same result).
  std::vector<CoreAccountId> ReorderChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& first_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts) const;

 private:
  // Reorders chrome accounts in the order they should appear in cookies with
  // respect to existing cookies.
  virtual std::vector<CoreAccountId> GetChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error,
      const gaia::MultiloginMode mode) const;

  // Returns Mode which shows if it is allowed to change the order of the gaia
  // accounts (e.g. on mobile or on stratup). Default is UPDATE.
  virtual gaia::MultiloginMode CalculateModeForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool primary_has_error) const;

  AccountReconcilor* reconcilor_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
