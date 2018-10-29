// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_pref_names.h"

namespace {
// Outputs accounts in the following order: first account, gaia accounts that
// are present in chrome_accounts, rest of chrome_accounts.
std::vector<std::string> ReorderChromeAccountsInternal(
    const std::vector<std::string>& chrome_accounts,
    const std::string& first_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  // Reordering should only happen if there is first account with a valid token.
  DCHECK(!first_account.empty());

  std::set<std::string> chrome_accounts_set(chrome_accounts.begin(),
                                            chrome_accounts.end());
  DCHECK(base::ContainsKey(chrome_accounts_set, first_account));

  std::vector<std::string> accounts_to_send;
  accounts_to_send.reserve(chrome_accounts.size());

  accounts_to_send.push_back(first_account);

  chrome_accounts_set.erase(first_account);
  for (const gaia::ListedAccount& gaia_account : gaia_accounts) {
    if (gaia_account.id == first_account ||
        chrome_accounts_set.find(gaia_account.id) == chrome_accounts_set.end())
      continue;
    accounts_to_send.push_back(gaia_account.id);
    chrome_accounts_set.erase(gaia_account.id);
  }

  for (const std::string& chrome_account : chrome_accounts_set) {
    accounts_to_send.push_back(chrome_account);
  }
  DCHECK(!accounts_to_send.empty());
  return accounts_to_send;
}
}  // namespace

namespace signin {

DiceAccountReconcilorDelegate::DiceAccountReconcilorDelegate(
    SigninClient* signin_client,
    AccountConsistencyMethod account_consistency)
    : signin_client_(signin_client), account_consistency_(account_consistency) {
  DCHECK(signin_client_);
}

bool DiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return DiceMethodGreaterOrEqual(account_consistency_,
                                  AccountConsistencyMethod::kDiceMigration);
}

bool DiceAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return account_consistency_ == AccountConsistencyMethod::kDice;
}

std::string DiceAccountReconcilorDelegate::GetGaiaApiSource() const {
  return "ChromiumAccountReconcilorDice";
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
std::string DiceAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution,
    bool will_logout) const {
  bool primary_account_has_token =
      !primary_account.empty() &&
      base::ContainsValue(chrome_accounts, primary_account);

  if (gaia_accounts.empty()) {
    if (primary_account_has_token)
      return primary_account;

    // Try the last known account. This happens when the cookies are cleared
    // while Sync is disabled.
    if (base::ContainsValue(chrome_accounts, last_known_first_account_))
      return last_known_first_account_;

    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? std::string() : chrome_accounts[0];
  }

  const std::string& first_gaia_account = gaia_accounts[0].id;
  bool first_gaia_account_has_token =
      base::ContainsValue(chrome_accounts, first_gaia_account);

  if (!first_gaia_account_has_token &&
      (primary_account == first_gaia_account) && gaia_accounts[0].valid) {
    // The primary account is also the first Gaia account, and has no token.
    // Logout everything.
    return std::string();
  }

  // If the primary Chrome account and the default Gaia account are both in
  // error, then the first gaia account can be kept, to avoid logging the user
  // out of their other accounts.
  // It's only possible when the reconcilor will not perform a logout, because
  // that account cannot be rebuilt.
  if (!primary_account_has_token && !gaia_accounts[0].valid && !will_logout)
    return first_gaia_account;

  if (first_execution) {
    // On first execution, try the primary account, and then the first Gaia
    // account.
    if (primary_account_has_token)
      return primary_account;
    if (first_gaia_account_has_token)
      return first_gaia_account;
    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? std::string() : chrome_accounts[0];
  }

  // While Chrome is running, try the first Gaia account, and then the
  // primary account.
  if (first_gaia_account_has_token)
    return first_gaia_account;
  if (primary_account_has_token)
    return primary_account;

  // Changing the first Gaia account while Chrome is running would be
  // confusing for the user. Logout everything.
  return std::string();
}

MultiloginMode DiceAccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string primary_account,
    bool first_execution,
    bool primary_has_error) const {
  const bool sync_enabled = !primary_account.empty();

  const bool first_gaia_is_primary =
      !gaia_accounts.empty() && (gaia_accounts[0].id == primary_account);

  const bool should_update_cookies = sync_enabled && first_execution &&
                                     !primary_has_error &&
                                     !first_gaia_is_primary;

  signin::MultiloginMode mode;
  if (should_update_cookies) {
    // UPDATE mode can happen only if sync is enabled.
    DCHECK(!primary_account.empty());
    mode = signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
  } else {
    mode = signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
  }
  return mode;
}

std::vector<std::string>
DiceAccountReconcilorDelegate::ReorderChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const signin::MultiloginMode mode) const {
  if (mode == signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER) {
    return ReorderChromeAccountsInternal(chrome_accounts, primary_account,
                                         gaia_accounts);
  }
  if (gaia_accounts.empty() &&
      base::ContainsValue(chrome_accounts, last_known_first_account_)) {
    // In PRESERVE mode in case accounts in cookies are accidentally lost we
    // should put cached first account first since Gaia has no information about
    // it.
    return ReorderChromeAccountsInternal(
        chrome_accounts, last_known_first_account_, gaia_accounts);
  }
  return chrome_accounts;
}

AccountReconcilorDelegate::RevokeTokenOption
DiceAccountReconcilorDelegate::ShouldRevokeSecondaryTokensBeforeReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  // During the Dice migration step, before Dice is actually enabled, chrome
  // tokens must be cleared when the cookies are cleared.
  if ((account_consistency_ == AccountConsistencyMethod::kDiceMigration) &&
      gaia_accounts.empty()) {
    return RevokeTokenOption::kRevoke;
  }

  return (account_consistency_ == AccountConsistencyMethod::kDice)
             ? RevokeTokenOption::kRevokeIfInError
             : RevokeTokenOption::kDoNotRevoke;
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted() {
  return account_consistency_ == AccountConsistencyMethod::kDice;
}

void DiceAccountReconcilorDelegate::OnReconcileFinished(
    const std::string& first_account,
    bool reconcile_is_noop) {
  last_known_first_account_ = first_account;

  // Migration happens on startup if the last reconcile was a no-op and the
  // refresh tokens are Dice-compatible.
  if (DiceMethodGreaterOrEqual(account_consistency_,
                               AccountConsistencyMethod::kDiceMigration)) {
    signin_client_->SetReadyForDiceMigration(
        reconcile_is_noop && signin_client_->GetPrefs()->GetBoolean(
                                 prefs::kTokenServiceDiceCompatible));
  }
}

}  // namespace signin
