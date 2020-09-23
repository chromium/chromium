// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"

const base::Feature kUseMultiloginEndpoint{"UseMultiloginEndpoint",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

namespace signin {

DiceAccountReconcilorDelegate::DiceAccountReconcilorDelegate(
    SigninClient* signin_client,
    bool migration_completed)
    : signin_client_(signin_client),
      migration_completed_(migration_completed) {
  DCHECK(signin_client_);
}

bool DiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

bool DiceAccountReconcilorDelegate::IsMultiloginEndpointEnabled() const {
  return base::FeatureList::IsEnabled(kUseMultiloginEndpoint);
}

bool DiceAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return true;
}

DiceAccountReconcilorDelegate::InconsistencyReason
DiceAccountReconcilorDelegate::GetInconsistencyReason(
    const CoreAccountId& primary_account,
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution) const {
  std::vector<CoreAccountId> valid_gaia_accounts_ids;
  for (const gaia::ListedAccount& gaia_account : gaia_accounts) {
    if (gaia_account.valid)
      valid_gaia_accounts_ids.push_back(gaia_account.id);
  }

  bool primary_account_has_token = false;
  if (!primary_account.empty()) {
    primary_account_has_token =
        base::Contains(chrome_accounts, primary_account);
    bool primary_account_has_cookie =
        base::Contains(valid_gaia_accounts_ids, primary_account);
    if (primary_account_has_token && !primary_account_has_cookie)
      return InconsistencyReason::kMissingSyncCookie;

    if (!primary_account_has_token && primary_account_has_cookie)
      return InconsistencyReason::kSyncAccountAuthError;
  }

  bool missing_first_web_account_token =
      primary_account.empty() && !gaia_accounts.empty() &&
      gaia_accounts[0].valid &&
      !base::Contains(chrome_accounts, gaia_accounts[0].id);

  if (missing_first_web_account_token)
    return InconsistencyReason::kMissingFirstWebAccountToken;

  std::sort(valid_gaia_accounts_ids.begin(), valid_gaia_accounts_ids.end());
  std::vector<CoreAccountId> sorted_chrome_accounts(chrome_accounts);
  std::sort(sorted_chrome_accounts.begin(), sorted_chrome_accounts.end());
  bool missing_token =
      !base::STLIncludes(sorted_chrome_accounts, valid_gaia_accounts_ids);
  bool missing_cookie =
      !base::STLIncludes(valid_gaia_accounts_ids, sorted_chrome_accounts);

  if (missing_token && missing_cookie)
    return InconsistencyReason::kCookieTokenMismatch;

  if (missing_token)
    return InconsistencyReason::kMissingSecondaryToken;

  if (missing_cookie)
    return InconsistencyReason::kMissingSecondaryCookie;

  if (first_execution && primary_account_has_token &&
      gaia_accounts[0].id != primary_account && gaia_accounts[0].valid)
    return InconsistencyReason::kSyncCookieNotFirst;

  return InconsistencyReason::kNone;
}

gaia::GaiaSource DiceAccountReconcilorDelegate::GetGaiaApiSource() const {
  return gaia::GaiaSource::kAccountReconcilorDice;
}

// - On first execution, the candidates are examined in this order:
//   1. The primary account
//   2. The current first Gaia account
//   3. The last known first Gaia account
//   4. The first account in the token service
// - On subsequent executions, the order is:
//   1. The current first Gaia account
//   2. The primary account
//   3. The last known first Gaia account
//   4. The first account in the token service
CoreAccountId DiceAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool will_logout) const {
  bool primary_account_has_token =
      !primary_account.empty() &&
      base::Contains(chrome_accounts, primary_account);

  if (gaia_accounts.empty()) {
    if (primary_account_has_token)
      return primary_account;

    // Try the last known account. This happens when the cookies are cleared
    // while Sync is disabled.
    if (base::Contains(chrome_accounts, last_known_first_account_))
      return last_known_first_account_;

    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? CoreAccountId() : chrome_accounts[0];
  }

  const CoreAccountId& first_gaia_account = gaia_accounts[0].id;
  bool first_gaia_account_has_token =
      base::Contains(chrome_accounts, first_gaia_account);

  if (!first_gaia_account_has_token &&
      (primary_account == first_gaia_account) && gaia_accounts[0].valid) {
    // The primary account is also the first Gaia account, and has no token.
    // Logout everything.
    return CoreAccountId();
  }

  // If the primary Chrome account and the default Gaia account are both in
  // error, then the first gaia account can be kept, to avoid logging the user
  // out of their other accounts.
  // It's only possible when the reconcilor will not perform a logout, because
  // that account cannot be rebuilt.
  if (!first_gaia_account_has_token && !gaia_accounts[0].valid && !will_logout)
    return first_gaia_account;

  if (first_execution) {
    // On first execution, try the primary account, and then the first Gaia
    // account.
    if (primary_account_has_token)
      return primary_account;
    if (first_gaia_account_has_token)
      return first_gaia_account;
    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? CoreAccountId() : chrome_accounts[0];
  }

  // While Chrome is running, try the first Gaia account, and then the
  // primary account.
  if (first_gaia_account_has_token)
    return first_gaia_account;
  if (primary_account_has_token)
    return primary_account;

  // Changing the first Gaia account while Chrome is running would be
  // confusing for the user. Logout everything.
  return CoreAccountId();
}

bool DiceAccountReconcilorDelegate::ShouldDeleteAccountsFromGaia(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts) const {
  // A valid Gaia account should be deleted if it does not have a Chrome
  // account.
  for (const gaia::ListedAccount& gaia_account : gaia_accounts) {
    if (gaia_account.valid && !base::Contains(chrome_accounts, gaia_account.id))
      return true;
  }
  return false;
}

bool DiceAccountReconcilorDelegate::IsPreserveModePossible(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    CoreAccountId first_account) const {
  if (ShouldDeleteAccountsFromGaia(chrome_accounts, gaia_accounts)) {
    // Preserve mode cannot remove accounts.
    return false;
  }
  // Check if the required first account is equal to the first account in
  // cookies as the preserve mode doesn't reorder cookies.
  return first_account.empty() || gaia_accounts.empty() ||
         first_account == gaia_accounts[0].id;
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensBeforeMultilogin(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error) const {
  // If Gaia accounts are empty, any combination of accounts can be set and
  // logout is not needed.
  if (gaia_accounts.empty())
    return false;

  // On first execution, it's generally OK to reorder accounts. Only logout if
  // the Sync account needs to be removed from the first position in cookies (it
  // would be unacceptable to swap another account there).
  if (first_execution) {
    return !primary_account.empty() && primary_has_error &&
           gaia_accounts[0].id == primary_account && gaia_accounts[0].valid;
  }
  // If there is a valid Sync account, then it's ok to reorder the accounts
  // even though Chrome is running (the accounts would be reordered on the next
  // startup, and this avoids a logout).
  if (!primary_account.empty() && !primary_has_error) {
    return false;
  }
  // If the first gaia account doesn't have token then logout. Exception: If the
  // first gaia account is invalid, but it can be left in its place (this is
  // possible only if there is no need to delete gaia accounts). Other accounts
  // will be added after.
  return !base::Contains(chrome_accounts, gaia_accounts[0].id) &&
         ShouldDeleteAccountsFromGaia(chrome_accounts, gaia_accounts);
}

CoreAccountId DiceAccountReconcilorDelegate::GetFirstGaiaAccountForMultilogin(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error) const {
  bool valid_sync_account = !primary_account.empty() && !primary_has_error;
  // On first execution if there is a valid sync account, then primary
  // account should be set to the first position.
  if (first_execution && valid_sync_account) {
    return primary_account;
  }
  // In case accounts in cookies are accidentally lost we
  // should try to put cached first account first since Gaia has no information
  // about it.
  if (gaia_accounts.empty() && !last_known_first_account_.empty() &&
      base::Contains(chrome_accounts, last_known_first_account_)) {
    // last_known_account_ is always empty on first execution.
    DCHECK(!first_execution);
    return last_known_first_account_;
  }
  // If there are no cookies and a valid sync account, then we can
  // set primary account to first position without reordering.
  if (gaia_accounts.empty() && valid_sync_account) {
    return primary_account;
  }
  // Empty account means that there is no special requirements for
  // the first account. The first account will be selected in such a way as
  // to minimize account re-numbering. ReorderChromeAccountsForReconcile
  // function implements this logic.
  return CoreAccountId();
}

std::vector<CoreAccountId>
DiceAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error,
    const gaia::MultiloginMode mode) const {
  CoreAccountId first_account = GetFirstGaiaAccountForMultilogin(
      chrome_accounts, primary_account, gaia_accounts, first_execution,
      primary_has_error);
  return ReorderChromeAccountsForReconcile(chrome_accounts, first_account,
                                           gaia_accounts);
}

// Minimize the use of Update, prefer using Preserve instead when possible.
gaia::MultiloginMode DiceAccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool primary_has_error) const {
  CoreAccountId first_account = GetFirstGaiaAccountForMultilogin(
      chrome_accounts, primary_account, gaia_accounts, first_execution,
      primary_has_error);
  return IsPreserveModePossible(chrome_accounts, gaia_accounts, first_account)
             ? gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER
             : gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
}

AccountReconcilorDelegate::RevokeTokenOption
DiceAccountReconcilorDelegate::ShouldRevokeSecondaryTokensBeforeReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  return RevokeTokenOption::kRevokeIfInError;
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensNotInCookies() const {
  return !migration_completed_;
}

void DiceAccountReconcilorDelegate::OnRevokeTokensNotInCookiesCompleted(
    RevokeTokenAction revoke_token_action) {
  migration_completed_ = true;
  signin_client_->SetDiceMigrationCompleted();
  UMA_HISTOGRAM_ENUMERATION("ForceDiceMigration.RevokeTokenAction",
                            revoke_token_action);
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted() {
  return true;
}

void DiceAccountReconcilorDelegate::OnReconcileFinished(
    const CoreAccountId& first_account) {
  last_known_first_account_ = first_account;
}

}  // namespace signin
