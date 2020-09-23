// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/account_consistency_method.h"

namespace signin {

MirrorAccountReconcilorDelegate::MirrorAccountReconcilorDelegate(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
  reconcile_enabled_ =
      identity_manager_->HasPrimaryAccount(GetConsentLevelForPrimaryAccount());
}

MirrorAccountReconcilorDelegate::~MirrorAccountReconcilorDelegate() {
  identity_manager_->RemoveObserver(this);
}

bool MirrorAccountReconcilorDelegate::IsReconcileEnabled() const {
  return reconcile_enabled_;
}

bool MirrorAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return true;
}

gaia::GaiaSource MirrorAccountReconcilorDelegate::GetGaiaApiSource() const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
}

bool MirrorAccountReconcilorDelegate::ShouldAbortReconcileIfPrimaryHasError()
    const {
  return true;
}

ConsentLevel MirrorAccountReconcilorDelegate::GetConsentLevelForPrimaryAccount()
    const {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(kMobileIdentityConsistency)) {
    return ConsentLevel::kNotRequired;
  }
#endif
  return ConsentLevel::kSync;
}

CoreAccountId MirrorAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool will_logout) const {
  // Mirror only uses the primary account, and it is never empty.
  DCHECK(!primary_account.empty());
  DCHECK(base::Contains(chrome_accounts, primary_account));
  return primary_account;
}

std::vector<CoreAccountId>
MirrorAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error,
    const gaia::MultiloginMode mode) const {
  DCHECK_EQ(mode,
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  return ReorderChromeAccountsForReconcile(chrome_accounts, primary_account,
                                           gaia_accounts);
}

// TODO(https://crbug.com/1046746): Replace separate IdentityManager::Observer
// method overrides below with a single OnPrimaryAccountChanged method and
// inline UpdateReconcilorStatus.
void MirrorAccountReconcilorDelegate::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdateReconcilorStatus();
}

void MirrorAccountReconcilorDelegate::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdateReconcilorStatus();
}

void MirrorAccountReconcilorDelegate::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  UpdateReconcilorStatus();
}

void MirrorAccountReconcilorDelegate::UpdateReconcilorStatus() {
  // Have to check whether the state has actually changed, as calling
  // DisableReconcile logs out all accounts even if it was already disabled.
  bool should_enable_reconcile =
      identity_manager_->HasPrimaryAccount(GetConsentLevelForPrimaryAccount());
  if (reconcile_enabled_ == should_enable_reconcile)
    return;

  reconcile_enabled_ = should_enable_reconcile;
  if (should_enable_reconcile) {
    reconcilor()->EnableReconcile();
  } else {
    reconcilor()->DisableReconcile(true /* logout_all_gaia_accounts */);
  }
}

}  // namespace signin
