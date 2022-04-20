// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace signin {

MirrorLandingAccountReconcilorDelegate::
    MirrorLandingAccountReconcilorDelegate() = default;

MirrorLandingAccountReconcilorDelegate::
    ~MirrorLandingAccountReconcilorDelegate() = default;

bool MirrorLandingAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

gaia::GaiaSource MirrorLandingAccountReconcilorDelegate::GetGaiaApiSource()
    const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
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
