// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/sign_in_promo_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

SignInPromoBubbleController::SignInPromoBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : delegate_(std::move(delegate)) {}

SignInPromoBubbleController::~SignInPromoBubbleController() = default;

Profile* SignInPromoBubbleController::GetProfile() const {
  content::WebContents* web_contents =
      delegate_ ? delegate_->GetWebContents() : nullptr;
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void SignInPromoBubbleController::OnSignInToChromeClicked(
    const AccountInfo& account) {
  // Enabling sync for an existing account and starting a new sign-in are
  // triggered by the user interacting with the sign-in promo.
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
  if (delegate_)
    delegate_->EnableSync(account);
}
