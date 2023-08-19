// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace signin {

MirrorLandingAccountReconcilorDelegate::MirrorLandingAccountReconcilorDelegate(
    IdentityManager* identity_manager,
    bool is_main_profile)
    : identity_manager_(identity_manager), is_main_profile_(is_main_profile) {}

MirrorLandingAccountReconcilorDelegate::
    ~MirrorLandingAccountReconcilorDelegate() = default;

bool MirrorLandingAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

gaia::GaiaSource MirrorLandingAccountReconcilorDelegate::GetGaiaApiSource()
    const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
}

bool MirrorLandingAccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted()
    const {
  // TODO(https://crbug.com.1464523): Migrate away from `ConsentLevel::kSync` on
  // Lacros.
  return !is_main_profile_ &&
         !identity_manager_->HasPrimaryAccount(ConsentLevel::kSync);
}

void MirrorLandingAccountReconcilorDelegate::
    OnAccountsCookieDeletedByUserAction(bool synced_data_deletion_in_progress) {
  if (!ShouldRevokeTokensOnCookieDeleted())
    return;

  identity_manager_->GetAccountsMutator()->RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_GaiaCookiesDeletedByUser);
}

bool MirrorLandingAccountReconcilorDelegate::
    ShouldAbortReconcileIfPrimaryHasError() const {
  return false;
}

ConsentLevel
MirrorLandingAccountReconcilorDelegate::GetConsentLevelForPrimaryAccount()
    const {
  return ConsentLevel::kSignin;
}

std::vector<CoreAccountId>
MirrorLandingAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error,
    const gaia::MultiloginMode mode) const {
  DCHECK_EQ(mode,
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  if (primary_has_error)
    return {};  // Log out all accounts.
  return ReorderChromeAccountsForReconcile(chrome_accounts, primary_account,
                                           gaia_accounts);
}

}  // namespace signin
