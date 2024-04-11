// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/consent_level.h"

namespace {

// User data key for AutofillSigninPromoTabHelper.
const void* const kAutofillSigninPromoTabHelperKey =
    &kAutofillSigninPromoTabHelperKey;

}  // namespace

namespace autofill {

AutofillSigninPromoTabHelper::AutofillSigninPromoTabHelper(
    content::WebContents& web_contents)
    : WebContentsUserData<AutofillSigninPromoTabHelper>(web_contents),
      state_(std::make_unique<ResetableState>(this)),
      web_contents_(&web_contents) {}

AutofillSigninPromoTabHelper::~AutofillSigninPromoTabHelper() {
  Reset();
}

AutofillSigninPromoTabHelper::ResetableState::ResetableState(
    signin::IdentityManager::Observer* observer)
    : identity_manager_observation_(observer) {}

AutofillSigninPromoTabHelper::ResetableState::~ResetableState() = default;

// static.
AutofillSigninPromoTabHelper* AutofillSigninPromoTabHelper::GetForWebContents(
    content::WebContents& web_contents) {
  if (!web_contents.GetUserData(kAutofillSigninPromoTabHelperKey)) {
    web_contents.SetUserData(
        kAutofillSigninPromoTabHelperKey,
        std::make_unique<AutofillSigninPromoTabHelper>(web_contents));
  }
  return static_cast<AutofillSigninPromoTabHelper*>(
      web_contents.GetUserData(kAutofillSigninPromoTabHelperKey));
}

void AutofillSigninPromoTabHelper::Reset() {
  state_ = std::make_unique<ResetableState>(this);
}

void AutofillSigninPromoTabHelper::InitializeDataMoveAfterSignIn(
    const password_manager::PasswordForm& password_form,
    signin_metrics::AccessPoint access_point,
    base::TimeDelta time_limit) {
  if (!state_->identity_manager_observation_.IsObserving()) {
    state_->identity_manager_observation_.Observe(
        IdentityManagerFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  }
  state_->password_form_ = password_form;
  state_->access_point_ = access_point;
  state_->time_limit_ = time_limit;
  state_->initialization_time_ = base::Time::Now();
  state_->is_initialized_ = true;
}

bool AutofillSigninPromoTabHelper::IsInitializedForTesting() const {
  return state_->is_initialized_;
}

void AutofillSigninPromoTabHelper::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Do not move the data anymore if the time limit since the initialization of
  // the move has been exceeded. This can happen for example if the user clicks
  // "Sign in" in the promo which opens a sign in tab and initializes this
  // helper, but the user does not complete the sign in. As they may forget that
  // this sign in tab would move the password, we do nothing instead.
  if (base::Time::Now() - state_->initialization_time_ > state_->time_limit_) {
    Reset();
    return;
  }

  // Return if there is no password to move.
  if (!state_->password_form_.HasNonEmptyPasswordValue()) {
    Reset();
    return;
  }

  // Do nothing if the primary account change was not a sign in event.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    Reset();
    return;
  }

  // We only want to move the data if the sign in event has the correct access
  // point, so if it was performed from the tab that was opened after clicking
  // the sign in promo.
  if (event_details.GetAccessPoint() != state_->access_point_) {
    Reset();
    return;
  }

  // Initiate password move if requirements are met.
  state_->move_helper_ =
      std::make_unique<password_manager::MovePasswordToAccountStoreHelper>(
          state_->password_form_,
          ChromePasswordManagerClient::FromWebContents(web_contents_),
          password_manager::metrics_util::MoveToAccountStoreTrigger::
              kUserOptedInAfterSavingLocally,
          // `base::Unretained(this)` is fine to use, because `this` owns the
          // `MovePasswordToAccountStoreHelper`.
          base::BindOnce(&AutofillSigninPromoTabHelper::Reset,
                         base::Unretained(this)));
}

void AutofillSigninPromoTabHelper::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillSigninPromoTabHelper);

}  // namespace autofill
