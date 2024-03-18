// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountReconcilor;

namespace signin {

// Base class for AccountReconcilorDelegate.
class AccountReconcilorDelegate {
 public:
  AccountReconcilorDelegate();
  virtual ~AccountReconcilorDelegate();

  // Returns true if the reconcilor should reconcile the profile. Defaults to
  // false.
  virtual bool IsReconcileEnabled() const;

  // Returns the value to set in the "source" parameter for Gaia API calls.
  virtual gaia::GaiaSource GetGaiaApiSource() const;

  // Returns true if Reconcile should be aborted when the primary account is in
  // error state. Defaults to false.
  virtual bool ShouldAbortReconcileIfPrimaryHasError() const;

  // Returns the consent level that should be used for obtaining the primary
  // account. Defaults to ConsentLevel::kSignin.
  virtual ConsentLevel GetConsentLevelForPrimaryAccount() const;

  // Returns a pair of mode and accounts to send to Mutilogin endpoint.
  MultiloginParameters CalculateParametersForMultilogin(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error) const;

  // Revokes secondary tokens if needed based on the platform.
  // Returns whether tokens has been revoked.
  virtual bool RevokeSecondaryTokensBeforeMultiloginIfNeeded(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution);

  // On Dice platforms:
  // - Revokes tokens in error state except the primary account with consent
  // level `GetConsentLevelForPrimaryAccount()`.
  // - If `IsUpdateCookieAllowed()` returns false, it also revokes tokens not
  // present in the gaia cookies to maintain account consistency.
  // On other platforms, this is no-op.
  virtual void RevokeSecondaryTokensForReconcileIfNeeded(
      const std::vector<gaia::ListedAccount>& gaia_accounts);

  // Called when cookies are deleted by user action.
  // This might be a no-op or signout the profile or lead to a sync paused state
  // based on different platforms conditions.
  virtual void OnAccountsCookieDeletedByUserAction(
      bool synced_data_deletion_in_progress);

  // Returns whether tokens should be revoked when the primary account is empty.
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

  raw_ptr<AccountReconcilor, DanglingUntriaged> reconcilor_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
