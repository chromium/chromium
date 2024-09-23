// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/common/buildflags.h"

namespace signin {

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
namespace {
bool IsAccountSupervised(IdentityManager* identity_manager) {
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return account_info.capabilities.is_subject_to_parental_controls() ==
         signin::Tribool::kTrue;
}
}  // namespace
#endif

// Revokes tokens for all accounts in chrome accounts but the primary account.
void RevokeAllSecondaryTokens(
    IdentityManager* identity_manager,
    ConsentLevel consent_level,
    signin_metrics::SourceForRefreshTokenOperation source,
    signin_metrics::ProfileSignout maybe_signout_source,
    bool revoke_only_if_in_error) {
  // If |consent_level| is sync but there is only a primary account with Signin
  // consent, it must be revoked.
  bool should_revoke_primary_account =
      consent_level == ConsentLevel::kSync &&
      !identity_manager->HasPrimaryAccount(ConsentLevel::kSync) &&
      identity_manager->HasPrimaryAccount(ConsentLevel::kSignin) &&
      (!revoke_only_if_in_error ||
       identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
           identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin)));

  if (should_revoke_primary_account) {
    // The primary account should be revoked by calling |ClearPrimaryAccount|.
    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        maybe_signout_source);
    DCHECK(identity_manager->GetAccountsWithRefreshTokens().empty());
    return;
  }
  // The sync account should not be removed but put in a paused state, therefore
  // this function excludes the primary account with sync consent. In some
  // cases, removing the primary account regardless of the consent level is not
  // allowed (e.g. cloud-managed enterprise profiles).
  CoreAccountId primary_account =
      identity_manager->GetPrimaryAccountId(consent_level);

  auto* accounts_mutator = identity_manager->GetAccountsMutator();
  for (const CoreAccountInfo& account_info :
       identity_manager->GetAccountsWithRefreshTokens()) {
    CoreAccountId account = account_info.account_id;

    bool is_primary_account =
        !primary_account.empty() && account == primary_account;

    if (is_primary_account)
      continue;

    bool should_revoke =
        !revoke_only_if_in_error ||
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account);

    if (should_revoke)
      accounts_mutator->RemoveAccount(account, source);
  }
}

DiceAccountReconcilorDelegate::DiceAccountReconcilorDelegate(
    IdentityManager* identity_manager,
    SigninClient* signin_client)
    : identity_manager_(identity_manager), signin_client_(signin_client) {}
DiceAccountReconcilorDelegate::~DiceAccountReconcilorDelegate() = default;

bool DiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

bool DiceAccountReconcilorDelegate::IsCookieBasedConsistencyMode() const {
  CHECK(IsReconcileEnabled());
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

void DiceAccountReconcilorDelegate::MatchTokensWithAccountsInCookie(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  CHECK(IsCookieBasedConsistencyMode());
  const signin_metrics::SourceForRefreshTokenOperation source =
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_RevokeTokensNotInCookies;
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  for (const CoreAccountInfo& account_info :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    auto it = base::ranges::find(gaia_accounts, account_info.account_id,
                                 &gaia::ListedAccount::id);
    if (it == gaia_accounts.end() || !it->valid) {
      // Account not in the cookie or the account is not valid (session
      // expired) and requires the user to reauth.
      accounts_mutator->RemoveAccount(account_info.account_id, source);
    }
  }
  // TODO(b/320279580): Record a histogram with the number of valid signed in
  // accounts in the cookie but doesn't have a refresh token (aka Chrome
  // account).
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
      !base::ranges::includes(sorted_chrome_accounts, valid_gaia_accounts_ids);
  bool missing_cookie =
      !base::ranges::includes(valid_gaia_accounts_ids, sorted_chrome_accounts);

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

bool DiceAccountReconcilorDelegate::
    RevokeSecondaryTokensBeforeMultiloginIfNeeded(
        const std::vector<CoreAccountId>& chrome_accounts,
        const std::vector<gaia::ListedAccount>& gaia_accounts,
        bool first_execution) {
  if (!ShouldRevokeTokensBeforeMultilogin(chrome_accounts, gaia_accounts,
                                          first_execution)) {
    return false;
  }

  RevokeAllSecondaryTokens(
      identity_manager_, GetConsentLevelForPrimaryAccount(),
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_Reconcile,
      signin_metrics::ProfileSignout::kAccountReconcilorReconcile,
      /*revoke_only_if_in_error=*/false);
  return true;
}

ConsentLevel DiceAccountReconcilorDelegate::GetConsentLevelForPrimaryAccount()
    const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // A supervised user regardless of consent should not be signed out in certain
  // cases such as clearing browsing data. In this instance the account
  // reconciler should not remove the primary account.
  if (IsAccountSupervised(identity_manager_)) {
    return ConsentLevel::kSignin;
  }
#endif

  if (!IsImplicitBrowserSigninOrExplicitDisabled(identity_manager_,
                                                 signin_client_->GetPrefs())) {
    return ConsentLevel::kSignin;
  }

  // In some cases, clearing the primary account is not allowed regardless of
  // the consent level (e.g. cloud-managed profiles). In these cases, the dice
  // account reconcilor delegate should never remove the primary account
  // regardless of the consent.
  // TODO(https://crbug.com.1464264): Migrate away from `ConsentLevel::kSync`
  // on desktop platforms.
  return signin_client_->IsClearPrimaryAccountAllowed(
             identity_manager_->HasPrimaryAccount(ConsentLevel::kSync))
             ? ConsentLevel::kSync
             : ConsentLevel::kSignin;
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensBeforeMultilogin(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution) const {
  // If Gaia accounts are empty, any combination of accounts can be set and
  // logout is not needed.
  if (gaia_accounts.empty()) {
    return false;
  }

  CoreAccountId primary_account = identity_manager_->GetPrimaryAccountId(
      GetConsentLevelForPrimaryAccount());

  bool primary_has_error =
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account);

  // On first execution, it's generally OK to reorder accounts. Only logout if
  // the primary account needs to be removed from the first position in cookies
  // (it would be unacceptable to swap another account there).
  if (first_execution) {
    return !primary_account.empty() && primary_has_error &&
           gaia_accounts[0].id == primary_account && gaia_accounts[0].valid;
  }
  // If there is a valid primary account, then it's ok to reorder the accounts
  // even though Chrome is running (the accounts would be reordered on the next
  // startup, and this avoids a logout).
  if (!primary_account.empty() && !primary_has_error) {
    return false;
  }

  // The default gaia account doesn't have token.
  if (!base::Contains(chrome_accounts, gaia_accounts[0].id)) {
    if (IsCookieBasedConsistencyMode()) {
      // Logout only if the default cookie account is valid.
      return gaia_accounts[0].valid;
    }

    // Logout with the exception: If the first gaia account is invalid, but it
    // can be left in its place (this is possible only if there is no need to
    // delete gaia accounts). Other accounts will be added after.
    return ShouldDeleteAccountsFromGaia(chrome_accounts, gaia_accounts);
  }
  return false;
}

CoreAccountId DiceAccountReconcilorDelegate::GetFirstGaiaAccountForMultilogin(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error) const {
  bool valid_primary_account = !primary_account.empty() && !primary_has_error;
  // On first execution if there is a valid primary account, then primary
  // account should be set to the first position.
  if (first_execution && valid_primary_account) {
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
  // If there are no cookies and a valid primary account, then we can
  // set primary account to first position without reordering.
  if (gaia_accounts.empty() && valid_primary_account) {
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

void DiceAccountReconcilorDelegate::RevokeSecondaryTokensForReconcileIfNeeded(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  RevokeAllSecondaryTokens(identity_manager_,
                           GetConsentLevelForPrimaryAccount(),
                           signin_metrics::SourceForRefreshTokenOperation::
                               kAccountReconcilor_GaiaCookiesUpdated,
                           signin_metrics::ProfileSignout::kGaiaCookieUpdated,
                           /*revoke_only_if_in_error=*/true);
  if (IsCookieBasedConsistencyMode()) {
    // Refresh tokens with no equivalent account in the cookie are revoked.
    MatchTokensWithAccountsInCookie(gaia_accounts);
  }
}

void DiceAccountReconcilorDelegate::OnAccountsCookieDeletedByUserAction(
    bool synced_data_deletion_in_progress) {
  ConsentLevel consent_level = GetConsentLevelForPrimaryAccount();
  // Revoke secondary tokens to avoid reconcilor rebuilding cookies.
  RevokeAllSecondaryTokens(
      identity_manager_, consent_level,
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_GaiaCookiesDeletedByUser,
      signin_metrics::ProfileSignout::kUserDeletedAccountCookies,
      /*revoke_only_if_in_error=*/false);

  if (!identity_manager_->HasPrimaryAccount(consent_level)) {
    return;
  }

  // In the explicit browser signin model the primary account should not be
  // signed out if authentication cookies are deleted by user action.
  if (AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
          *identity_manager_, *signin_client_->GetPrefs())) {
    return;
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (IsAccountSupervised(identity_manager_)) {
    return;
  }
#endif

  if (synced_data_deletion_in_progress &&
      identity_manager_->HasPrimaryAccount(ConsentLevel::kSync)) {
    // If sync data deletion in progress, avoid invalidating the sync
    // account unless it is already in a persistent error state. This is needed
    // to ensure the data gets deleted from the google account.
    CoreAccountId primary_sync_account =
        identity_manager_->GetPrimaryAccountId(ConsentLevel::kSync);
    if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_sync_account)) {
      return;
    }
  }

  // The primary account should be paused if the account cookie is deleted by
  // user action.
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  accounts_mutator->InvalidateRefreshTokenForPrimaryAccount(
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_GaiaCookiesDeletedByUser);
}

void DiceAccountReconcilorDelegate::OnReconcileFinished(
    const CoreAccountId& first_account) {
  last_known_first_account_ = first_account;
}

}  // namespace signin
