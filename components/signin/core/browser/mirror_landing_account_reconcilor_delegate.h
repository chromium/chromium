// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_LANDING_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_LANDING_ACCOUNT_RECONCILOR_DELEGATE_H_

#include "components/signin/core/browser/account_reconcilor_delegate.h"

namespace signin {

class IdentityManager;

// AccountReconcilorDelegate specialized for Mirror, using the "Mirror landing"
// variant. Mirror is always enabled, even when there is no primary account.
class MirrorLandingAccountReconcilorDelegate
    : public AccountReconcilorDelegate {
 public:
  MirrorLandingAccountReconcilorDelegate(IdentityManager* identity_manager,
                                         bool is_main_profile);
  ~MirrorLandingAccountReconcilorDelegate() override;

  MirrorLandingAccountReconcilorDelegate(
      const MirrorLandingAccountReconcilorDelegate&) = delete;
  MirrorLandingAccountReconcilorDelegate& operator=(
      const MirrorLandingAccountReconcilorDelegate&) = delete;

  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  gaia::GaiaSource GetGaiaApiSource() const override;
  bool ShouldAbortReconcileIfPrimaryHasError() const override;
  ConsentLevel GetConsentLevelForPrimaryAccount() const override;
  void OnAccountsCookieDeletedByUserAction(
      bool synced_data_deletion_in_progress) override;
  std::vector<CoreAccountId> GetChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error,
      const gaia::MultiloginMode mode) const override;

 private:
  bool ShouldRevokeTokensOnCookieDeleted() const;
  const raw_ptr<IdentityManager> identity_manager_;
  const bool is_main_profile_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_LANDING_ACCOUNT_RECONCILOR_DELEGATE_H_
