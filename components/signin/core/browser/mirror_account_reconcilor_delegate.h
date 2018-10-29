// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_ACCOUNT_RECONCILOR_DELEGATE_H_

#include "base/macros.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/signin_manager_base.h"

namespace signin {

// AccountReconcilorDelegate specialized for Mirror.
class MirrorAccountReconcilorDelegate : public AccountReconcilorDelegate,
                                        public SigninManagerBase::Observer {
 public:
  MirrorAccountReconcilorDelegate(SigninManagerBase* signin_manager);
  ~MirrorAccountReconcilorDelegate() override;

 private:
  // AccountReconcilorDelegate:
  bool IsReconcileEnabled() const override;
  bool IsAccountConsistencyEnforced() const override;
  std::string GetGaiaApiSource() const override;
  bool ShouldAbortReconcileIfPrimaryHasError() const override;
  std::string GetFirstGaiaAccountForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string& primary_account,
      bool first_execution,
      bool will_logout) const override;

  std::vector<std::string> ReorderChromeAccountsForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::string& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const signin::MultiloginMode mode) const override;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  SigninManagerBase* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(MirrorAccountReconcilorDelegate);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_MIRROR_ACCOUNT_RECONCILOR_DELEGATE_H_
