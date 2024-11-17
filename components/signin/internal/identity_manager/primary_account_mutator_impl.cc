// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {

PrimaryAccountMutatorImpl::PrimaryAccountMutatorImpl(
    AccountTrackerService* account_tracker,
    PrimaryAccountManager* primary_account_manager,
    PrefService* pref_service,
    SigninClient* signin_client)
    : account_tracker_(account_tracker),
      primary_account_manager_(primary_account_manager),
      pref_service_(pref_service),
      signin_client_(signin_client) {
  DCHECK(account_tracker_);
  DCHECK(primary_account_manager_);
  DCHECK(pref_service_);
  DCHECK(signin_client_);
}

PrimaryAccountMutatorImpl::~PrimaryAccountMutatorImpl() = default;

PrimaryAccountMutator::PrimaryAccountError
PrimaryAccountMutatorImpl::SetPrimaryAccount(
    const CoreAccountId& account_id,
    ConsentLevel consent_level,
    signin_metrics::AccessPoint access_point,
    base::OnceClosure prefs_committed_callback) {
  DCHECK(!account_id.empty());
  AccountInfo account_info = account_tracker_->GetAccountInfo(account_id);
  if (account_info.IsEmpty())
    return PrimaryAccountError::kAccountInfoEmpty;

  DCHECK_EQ(account_info.account_id, account_id);
  DCHECK(!account_info.email.empty());
  DCHECK(!account_info.gaia.empty());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_signin_allowed = pref_service_->GetBoolean(prefs::kSigninAllowed);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Check that `prefs::kSigninAllowed` has not been set to false in a context
  // where Lacros wants to set a Primary Account. Lacros doesn't offer account
  // inconsistency - just like Ash.
  DCHECK(is_signin_allowed);
#endif
  if (!is_signin_allowed)
    return PrimaryAccountError::kSigninNotAllowed;
#endif

  switch (consent_level) {
    case ConsentLevel::kSync:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(crbug.com/40067025): Replace with NOTREACHED on iOS after all
      // flows have been migrated away from kSync. See ConsentLevel::kSync
      // documentation for details.
      if (primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync))
        return PrimaryAccountError::kSyncConsentAlreadySet;
#endif
      break;
    case ConsentLevel::kSignin:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // On Chrome OS the UPA can only be set once and never removed or changed.
      DCHECK(
          !primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin));
#endif
      // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
      //     deleted. See ConsentLevel::kSync documentation for details.
      DCHECK(!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync));
      break;
  }
  if (primary_account_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kSignin) &&
      account_info.account_id != primary_account_manager_->GetPrimaryAccountId(
                                     signin::ConsentLevel::kSignin) &&
      !signin_client_->IsClearPrimaryAccountAllowed(
          /*has_sync_account=*/false)) {
    DVLOG(1) << "Changing the primary account is not allowed.";
    return PrimaryAccountError::kPrimaryAccountChangeNotAllowed;
  }

  primary_account_manager_->SetPrimaryAccountInfo(
      account_info, consent_level, access_point,
      std::move(prefs_committed_callback));
  return PrimaryAccountError::kNoError;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Users cannot revoke the Sync consent on Ash. They can only turn off all Sync
// data types if they want. Revoking sync consent can lead to breakages in
// IdentityManager dependencies like `chrome.identity` extension API - that
// assume that an account will always be available at sync consent level in Ash.
void PrimaryAccountMutatorImpl::RevokeSyncConsent(
    signin_metrics::ProfileSignout source_metric) {
  // TODO(crbug.com/40066949): `RevokeSyncConsent` shouldn't be available on iOS
  //     when kSync is no longer used. See ConsentLevel::kSync documentation for
  //     details.
  DCHECK(primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync));
  primary_account_manager_->RevokeSyncConsent(source_metric);
}

bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    signin_metrics::ProfileSignout source_metric) {
  if (!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin))
    return false;

  primary_account_manager_->ClearPrimaryAccount(source_metric);
  return true;
}

bool PrimaryAccountMutatorImpl::RemovePrimaryAccountButKeepTokens(
    signin_metrics::ProfileSignout source_metric) {
  if (!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin)) {
    return false;
  }

  primary_account_manager_->RemovePrimaryAccountButKeepTokens(source_metric);
  return true;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace signin
