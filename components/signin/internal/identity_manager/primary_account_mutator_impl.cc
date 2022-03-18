// Copyright 2018 The Chromium Authors. All rights reserved.
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
PrimaryAccountMutatorImpl::SetPrimaryAccount(const CoreAccountId& account_id,
                                             ConsentLevel consent_level) {
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
  primary_account_manager_->SetPrimaryAccountInfo(account_info, consent_level);
  return PrimaryAccountError::kNoError;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool PrimaryAccountMutatorImpl::CanTransitionFromSyncToSigninConsentLevel()
    const {
  switch (account_consistency_) {
    case AccountConsistencyMethod::kDice:
      // If DICE is enabled, then adding and removing accounts is handled from
      // the Google web services. This means that the user needs to be signed
      // in to the their Google account on the web in order to be able to sign
      // out of that accounts. As in most cases, the Google auth cookies are
      // are derived from the refresh token, which means that the user is signed
      // out of their Google account on the web when the primary account is in
      // an auth error. It is therefore important to clear all accounts when
      // the user revokes their sync consent for a primary account that is in
      // an auth error as otherwise the user will not be able to remove it from
      // Chrome.
      //
      // TODO(msarda): The logic in this function is platform specific and we
      // should consider moving it to |SigninManager|.
      return !token_service_->RefreshTokenHasError(
          primary_account_manager_->GetPrimaryAccountId(ConsentLevel::kSync));
    case AccountConsistencyMethod::kMirror:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // TODO(crbug.com/1217645): Consider making this return false only for the
      // main profile and return true, otherwise. This requires implementing
      // ProfileOAuth2TokenServiceDelegateChromeOS::Revoke* and it's not clear
      // what these functions should do.
      return true;
#elif BUILDFLAG(IS_ANDROID)
      // Android supports users being signed in with sync disabled, with the
      // exception of child accounts with the kAllowSyncOffForChildAccounts
      // flag disabled.
      //
      // Strictly-speaking we should only look at the value of this flag for
      // child accounts, however the child account status is not easily
      // available here and it doesn't matter if we clear the primary account
      // for non-Child accounts as we don't expose a 'Turn off sync' UI for
      // them.  As this is a short-lived flag, we leave as-is rather than
      // plumb through child status here.
      return base::FeatureList::IsEnabled(
          switches::kAllowSyncOffForChildAccounts);
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

void PrimaryAccountMutatorImpl::RevokeSyncConsent(
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  DCHECK(primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!CanTransitionFromSyncToSigninConsentLevel()) {
    ClearPrimaryAccount(source_metric, delete_metric);
    return;
  }
#endif
  primary_account_manager_->RevokeSyncConsent(source_metric, delete_metric);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  if (!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin))
    return false;

  primary_account_manager_->ClearPrimaryAccount(source_metric, delete_metric);
  return true;
}
#endif

}  // namespace signin
