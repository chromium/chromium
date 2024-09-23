// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/core/browser/account_reconcilor.h"

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

gaia::GaiaSource MirrorAccountReconcilorDelegate::GetGaiaApiSource() const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
}

bool MirrorAccountReconcilorDelegate::ShouldAbortReconcileIfPrimaryHasError()
    const {
  return true;
}

ConsentLevel MirrorAccountReconcilorDelegate::GetConsentLevelForPrimaryAccount()
    const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40067189): Migrate away from `ConsentLevel::kSync` on
  // Ash.
  return ConsentLevel::kSync;
#else
  // For mobile (iOS, Android) and Lacros.
  //
  // Whenever Mirror is enabled on a Lacros Profile, the Primary Account may or
  // may not have consented to Chrome Sync. But we want to enable
  // `AccountReconcilor` regardless - for minting Gaia cookies.
  return ConsentLevel::kSignin;
#endif
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

void MirrorAccountReconcilorDelegate::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event) {
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
