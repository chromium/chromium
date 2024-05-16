// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillBubbleSignInPromoController::AutofillBubbleSignInPromoController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    const password_manager::PasswordForm& saved_password)
    : delegate_(std::move(delegate)), saved_password_(saved_password) {}

AutofillBubbleSignInPromoController::~AutofillBubbleSignInPromoController() =
    default;

void AutofillBubbleSignInPromoController::OnSignInToChromeClicked(
    const AccountInfo& account) {
  // Signing in is triggered by the user interacting with the sign-in promo.
  if (delegate_) {
    delegate_->SignIn(account, saved_password_);
  }
}

}  // namespace autofill
