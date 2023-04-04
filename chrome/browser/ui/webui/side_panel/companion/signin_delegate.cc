// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/signin_delegate.h"

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace companion {
namespace {
class SigninDelegateImpl : public SigninDelegate {
 public:
  explicit SigninDelegateImpl(Profile* profile);
  ~SigninDelegateImpl() override;

  bool AllowedSignin() override;
  void StartSigninFlow() override;

 private:
  raw_ptr<Profile> profile_;
};

SigninDelegateImpl::SigninDelegateImpl(Profile* profile) : profile_(profile) {}

SigninDelegateImpl::~SigninDelegateImpl() = default;

bool SigninDelegateImpl::AllowedSignin() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  if (!SyncServiceFactory::IsSyncAllowed(profile_)) {
    return false;
  }

  // Check if already signed in.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  return false;
}

void SigninDelegateImpl::StartSigninFlow() {
  DCHECK(AllowedSignin());

  // Show the promo here.
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile_,
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION);
}

}  // namespace

// static
std::unique_ptr<SigninDelegate> SigninDelegate::Create(Profile* profile) {
  return std::make_unique<SigninDelegateImpl>(profile);
}

}  // namespace companion
