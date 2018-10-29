// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountReconcilor;

namespace signin {

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

  // Returns true if account consistency is enforced (Mirror or Dice).
  // If this is false, reconcile is done, but its results are discarded and no
  // changes to the accounts are made. Defaults to false.
  virtual bool IsAccountConsistencyEnforced() const;

  // Returns the value to set in the "source" parameter for Gaia API calls.
  virtual std::string GetGaiaApiSource() const;

  // Returns true if Reconcile should be aborted when the primary account is in
  // error state. Defaults to false.
  virtual bool ShouldAbortReconcileIfPrimaryHasError() const;

  // Returns the first account to add in the Gaia cookie.
  // If this returns an empty string, the user must be logged out of all
  // accounts.
  // |first_execution| is true for the first reconciliation after startup.
  // |will_logout| is true if the reconcilor will perform a logout no matter
  // what is returned by this function.
  // Only used with MergeSession.
  virtual std::string GetFirstGaiaAccountForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string& primary_account,
      bool first_execution,
      bool will_logout) const;

  // Returns a pair of mode and accounts to send to Mutilogin endpoint.
  MultiloginParameters CalculateParametersForMultilogin(
      const std::vector<std::string>& chrome_accounts,
      const std::string& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error) const;

  // Returns whether secondary accounts should be revoked at the beginning of
  // the reconcile.
  virtual RevokeTokenOption ShouldRevokeSecondaryTokensBeforeReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts);

  // Returns whether tokens should be revoked when the Gaia cookie has been
  // explicitly deleted by the user.
  // If this returns false, tokens will not be revoked. If this returns true,
  // secondary tokens will be deleted ; and the primary token will be
  // invalidated unless it has to be kept for critical Sync operations.
  virtual bool ShouldRevokeTokensOnCookieDeleted();

  // Called when reconcile is finished.
  // |OnReconcileFinished| is always called at the end of reconciliation, even
  // when there is an error (except in cases where reconciliation times out
  // before finishing, see |GetReconcileTimeout|).
  virtual void OnReconcileFinished(const std::string& first_account,
                                   bool reconcile_is_noop) {}

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

  void set_reconcilor(AccountReconcilor* reconcilor) {
    reconcilor_ = reconcilor;
  }
  AccountReconcilor* reconcilor() { return reconcilor_; }

 private:
  // Reorders chrome accounts in the order they should appear in cookies with
  // respect to existing cookies.
  virtual std::vector<std::string> ReorderChromeAccountsForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::string& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const signin::MultiloginMode mode) const;

  // Returns Mode which shows if it is allowed to change the order of the gaia
  // accounts (e.g. on mobile or on stratup). Default is UPDATE.
  virtual MultiloginMode CalculateModeForReconcile(
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string primary_account,
      bool first_execution,
      bool primary_has_error) const;

  AccountReconcilor* reconcilor_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
