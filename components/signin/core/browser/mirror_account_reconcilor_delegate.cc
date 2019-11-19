// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_reconcilor.h"

namespace signin {

MirrorAccountReconcilorDelegate::MirrorAccountReconcilorDelegate(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
}

MirrorAccountReconcilorDelegate::~MirrorAccountReconcilorDelegate() {
  identity_manager_->RemoveObserver(this);
}

bool MirrorAccountReconcilorDelegate::IsReconcileEnabled() const {
  return identity_manager_->HasPrimaryAccount();
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
    const gaia::MultiloginMode mode) const {
  DCHECK_EQ(mode,
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  return ReorderChromeAccountsForReconcile(chrome_accounts, primary_account,
                                           gaia_accounts);
}

void MirrorAccountReconcilorDelegate::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  reconcilor()->EnableReconcile();
}

void MirrorAccountReconcilorDelegate::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  reconcilor()->DisableReconcile(true /* logout_all_gaia_accounts */);
}

}  // namespace signin
