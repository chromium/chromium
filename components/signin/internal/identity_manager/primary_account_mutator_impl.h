// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

class AccountTrackerService;
class PrefService;
class PrimaryAccountManager;
class SigninClient;

namespace signin {

// Concrete implementation of PrimaryAccountMutator that is based on the
// PrimaryAccountManager API.
class PrimaryAccountMutatorImpl : public PrimaryAccountMutator {
 public:
  PrimaryAccountMutatorImpl(AccountTrackerService* account_tracker,
                            PrimaryAccountManager* primary_account_manager,
                            PrefService* pref_service,
                            SigninClient* signin_client);
  ~PrimaryAccountMutatorImpl() override;

  // PrimaryAccountMutator implementation.
  PrimaryAccountError SetPrimaryAccount(
      const CoreAccountId& account_id,
      ConsentLevel consent_level,
      signin_metrics::AccessPoint access_point,
      base::OnceClosure prefs_committed_callback) override;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void RevokeSyncConsent(signin_metrics::ProfileSignout source_metric) override;
  bool ClearPrimaryAccount(
      signin_metrics::ProfileSignout source_metric) override;
  bool RemovePrimaryAccountButKeepTokens(
      signin_metrics::ProfileSignout source_metric) override;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns true if transitioning from Sync to Signin consent level is allowed
  // for this platform / configuration.
  bool CanTransitionFromSyncToSigninConsentLevel() const;
#endif

  // Pointers to the services used by the PrimaryAccountMutatorImpl. They
  // *must* outlive this instance.
  raw_ptr<AccountTrackerService> account_tracker_ = nullptr;
  raw_ptr<PrimaryAccountManager> primary_account_manager_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<SigninClient> signin_client_ = nullptr;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
