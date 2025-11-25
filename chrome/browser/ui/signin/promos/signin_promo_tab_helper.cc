// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/consent_level.h"

namespace {

// User data key for SigninPromoTabHelper.
const void* const kSigninPromoTabHelperKey = &kSigninPromoTabHelperKey;

}  // namespace

SigninPromoTabHelper::SigninPromoTabHelper(content::WebContents& web_contents)
    : WebContentsUserData<SigninPromoTabHelper>(web_contents),
      state_(std::make_unique<ResetableState>(this)),
      web_contents_(&web_contents) {}

SigninPromoTabHelper::~SigninPromoTabHelper() {
  Reset();
}

SigninPromoTabHelper::ResetableState::ResetableState(
    signin::IdentityManager::Observer* observer)
    : identity_manager_observation_(observer) {}

SigninPromoTabHelper::ResetableState::~ResetableState() = default;

// static.
SigninPromoTabHelper* SigninPromoTabHelper::GetForWebContents(
    content::WebContents& web_contents) {
  if (!web_contents.GetUserData(kSigninPromoTabHelperKey)) {
    web_contents.SetUserData(
        kSigninPromoTabHelperKey,
        std::make_unique<SigninPromoTabHelper>(web_contents));
  }
  return static_cast<SigninPromoTabHelper*>(
      web_contents.GetUserData(kSigninPromoTabHelperKey));
}

void SigninPromoTabHelper::Reset() {
  state_ = std::make_unique<ResetableState>(this);
}

void SigninPromoTabHelper::InitializeCallbackAfterSignIn(
    base::OnceClosure completed_callback,
    signin_metrics::AccessPoint access_point,
    base::TimeDelta time_limit) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!state_->identity_manager_observation_.IsObserving()) {
    state_->identity_manager_observation_.Observe(identity_manager);
  }

  // If there is already an account in error state, then we need to reauth. This
  // variable will help us determine whether we should reset the helper in
  // `OnErrorStateOfRefreshTokenUpdatedForAccount` or not.
  state_->needs_reauth_ = signin_util::IsSigninPending(identity_manager);
  state_->completed_callback_ = std::move(completed_callback);
  state_->access_point_ = access_point;
  state_->time_limit_ = time_limit;
  state_->initialization_time_ = base::Time::Now();
  state_->is_initialized_ = true;
}

bool SigninPromoTabHelper::IsInitializedForTesting() const {
  return state_->is_initialized_;
}

void SigninPromoTabHelper::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  // Do nothing if there was not already an account with a refresh token error
  // before. The data move will be initiated by `OnPrimaryAccountChanged`.
  if (!state_->needs_reauth_) {
    return;
  }

  // Do not run the callback anymore if the time limit since the initialization
  // has been exceeded. This can happen for example if the user clicks "Sign in"
  // in the promo which opens a sign in tab and initializes this helper, but the
  // user does not complete the sign in. As they may forget that this sign in
  // tab would do something (e.g. move data), we do nothing instead.
  if (base::Time::Now() - state_->initialization_time_ > state_->time_limit_) {
    Reset();
    return;
  }

  // Do nothing if there is still an error.
  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    Reset();
    return;
  }

  // Do nothing if the error was not resolved for the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    Reset();
    return;
  }

  // We only want to run the callback if the sign in event has the correct
  // access point, so if it was performed from the tab that was opened after
  // clicking the sign in promo.
  if (identity_manager->FindExtendedAccountInfo(account_info).access_point !=
      state_->access_point_) {
    Reset();
    return;
  }

  // Run the callback if requirements are met.
  std::move(state_->completed_callback_).Run();
  Reset();
}

void SigninPromoTabHelper::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // The user changed their primary account while we were expecting them to
  // reauthenticate. Abandon the callback, e.g. a data move, to avoid saving
  // data to a wrong account.
  if (state_->needs_reauth_) {
    Reset();
    return;
  }

  // Do not run the callback anymore if the time limit since the initialization
  // has been exceeded. This can happen for example if the user clicks "Sign in"
  // in the promo which opens a sign in tab and initializes this helper, but the
  // user does not complete the sign in. As they may forget that this sign in
  // tab would do something (e.g. move data), we do nothing instead.
  if (base::Time::Now() - state_->initialization_time_ > state_->time_limit_) {
    Reset();
    return;
  }

  // Do nothing if the primary account change was not a sign in event.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    Reset();
    return;
  }

  // We only want to run the callback if the sign in event has the correct
  // access point, so if it was performed from the tab that was opened after
  // clicking the sign in promo.
  if (event_details.GetSetPrimaryAccountAccessPoint() !=
      state_->access_point_) {
    Reset();
    return;
  }

  // Run the callback if requirements are met.
  std::move(state_->completed_callback_).Run();
  Reset();
}

void SigninPromoTabHelper::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SigninPromoTabHelper);
