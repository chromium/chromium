// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_

#include <string>

#include "components/signin/public/identity_manager/primary_account_mutator.h"

class AccountTrackerService;
class PrefService;
class PrimaryAccountManager;

namespace signin {

// Concrete implementation of PrimaryAccountMutator that is based on the
// PrimaryAccountManager API.
class PrimaryAccountMutatorImpl : public PrimaryAccountMutator {
 public:
  PrimaryAccountMutatorImpl(AccountTrackerService* account_tracker,
                            PrimaryAccountManager* primary_account_manager,
                            PrefService* pref_service);
  ~PrimaryAccountMutatorImpl() override;

  // PrimaryAccountMutator implementation.
  bool SetPrimaryAccount(const CoreAccountId& account_id) override;
#if defined(OS_CHROMEOS)
  bool DeprecatedSetPrimaryAccountAndUpdateAccountInfo(
      const std::string& gaia_id,
      const std::string& email) override;
#endif
#if !defined(OS_CHROMEOS)
  bool ClearPrimaryAccount(
      ClearAccountsAction action,
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) override;
#endif

 private:
  // Pointers to the services used by the PrimaryAccountMutatorImpl. They
  // *must* outlive this instance.
  AccountTrackerService* account_tracker_ = nullptr;
  PrimaryAccountManager* primary_account_manager_ = nullptr;
  PrefService* pref_service_ = nullptr;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
