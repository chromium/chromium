// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {

PrimaryAccountMutatorImpl::PrimaryAccountMutatorImpl(
    AccountTrackerService* account_tracker,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager,
    PrefService* pref_service,
    signin::AccountConsistencyMethod account_consistency)
    : account_tracker_(account_tracker),
      token_service_(token_service),
      primary_account_manager_(primary_account_manager),
      pref_service_(pref_service),
      account_consistency_(account_consistency) {
  DCHECK(account_tracker_);
  DCHECK(token_service_);
  DCHECK(primary_account_manager_);
  DCHECK(pref_service_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // |account_consistency_| is not used on CHROMEOS_ASH, however it is preferred
  // to have it defined to avoid a lot of ifdefs in the header file.
  [[maybe_unused]] signin::AccountConsistencyMethod unused =
      account_consistency_;
#endif
}

PrimaryAccountMutatorImpl::~PrimaryAccountMutatorImpl() {}

PrimaryAccountMutator::PrimaryAccountError
PrimaryAccountMutatorImpl::SetPrimaryAccount(
    const CoreAccountId& account_id,
    ConsentLevel consent_level,
    signin_metrics::AccessPoint access_point) {
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
      DCHECK(!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync));
      break;
  }
  primary_account_manager_->SetPrimaryAccountInfo(account_info, consent_level,
                                                  access_point);
  return PrimaryAccountError::kNoError;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool PrimaryAccountMutatorImpl::CanTransitionFromSyncToSigninConsentLevel()
    const {
  switch (account_consistency_) {
    case AccountConsistencyMethod::kDice:
      return true;
    case AccountConsistencyMethod::kMirror:
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_ANDROID)
      return true;
#else
      // TODO(crbug.com/1165785): once kAllowSyncOffForChildAccounts has been
      // rolled out and assuming it has not revealed any issues, make the
      // behaviour consistent across all Mirror platforms, by allowing this
      // transition on iOS too (i.e. return true with no platform checks for
      // kMirror).
      return false;
#endif
    case AccountConsistencyMethod::kDisabled:
      return false;
  }
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Users cannot revoke the Sync consent on Ash. They can only turn off all Sync
// data types if they want. Revoking sync consent can lead to breakages in
// IdentityManager dependencies like `chrome.identity` extension API - that
// assume that an account will always be available at sync consent level in Ash.
void PrimaryAccountMutatorImpl::RevokeSyncConsent(
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  DCHECK(primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync));

  if (!CanTransitionFromSyncToSigninConsentLevel()) {
    ClearPrimaryAccount(source_metric, delete_metric);
    return;
  }
  primary_account_manager_->RevokeSyncConsent(source_metric, delete_metric);
}

bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  if (!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin))
    return false;

  primary_account_manager_->ClearPrimaryAccount(source_metric, delete_metric);
  return true;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace signin
