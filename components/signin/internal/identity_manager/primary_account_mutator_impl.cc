// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"

#include <string>

#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {

PrimaryAccountMutatorImpl::PrimaryAccountMutatorImpl(
    AccountTrackerService* account_tracker,
    PrimaryAccountManager* primary_account_manager,
    PrefService* pref_service)
    : account_tracker_(account_tracker),
      primary_account_manager_(primary_account_manager),
      pref_service_(pref_service) {
  DCHECK(account_tracker_);
  DCHECK(primary_account_manager_);
  DCHECK(pref_service_);
}

PrimaryAccountMutatorImpl::~PrimaryAccountMutatorImpl() {}

bool PrimaryAccountMutatorImpl::SetPrimaryAccount(
    const CoreAccountId& account_id) {
  AccountInfo account_info = account_tracker_->GetAccountInfo(account_id);

#if !defined(OS_CHROMEOS)
  if (!pref_service_->GetBoolean(prefs::kSigninAllowed))
    return false;

  if (primary_account_manager_->IsAuthenticated())
    return false;

  if (account_info.account_id != account_id || account_info.email.empty())
    return false;

  // TODO(crbug.com/889899): should check that the account email is allowed.
#endif

  primary_account_manager_->SignIn(account_info.email);
  return true;
}

void PrimaryAccountMutatorImpl::SetUnconsentedPrimaryAccount(
    const CoreAccountId& account_id) {
#if defined(OS_CHROMEOS)
  // On Chrome OS the UPA can only be set once and never removed or changed.
  DCHECK(!account_id.empty());
  DCHECK(!primary_account_manager_->HasUnconsentedPrimaryAccount());
#endif
  AccountInfo account_info;
  if (!account_id.empty()) {
    account_info = account_tracker_->GetAccountInfo(account_id);
    DCHECK(!account_info.IsEmpty());
  }

  primary_account_manager_->SetUnconsentedPrimaryAccountInfo(account_info);
}

#if defined(OS_CHROMEOS)
void PrimaryAccountMutatorImpl::RevokeSyncConsent() {
  primary_account_manager_->RevokeSyncConsent();
}
#endif

#if !defined(OS_CHROMEOS)
bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    ClearAccountsAction action,
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  if (!primary_account_manager_->HasUnconsentedPrimaryAccount())
    return false;

  switch (action) {
    case PrimaryAccountMutator::ClearAccountsAction::kDefault:
      primary_account_manager_->SignOut(source_metric, delete_metric);
      break;
    case PrimaryAccountMutator::ClearAccountsAction::kKeepAll:
      primary_account_manager_->SignOutAndKeepAllAccounts(source_metric,
                                                          delete_metric);
      break;
    case PrimaryAccountMutator::ClearAccountsAction::kRemoveAll:
      primary_account_manager_->SignOutAndRemoveAllAccounts(source_metric,
                                                            delete_metric);
      break;
  }

  return true;
}
#endif

}  // namespace signin
