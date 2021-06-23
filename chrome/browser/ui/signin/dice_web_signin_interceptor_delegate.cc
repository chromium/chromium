// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "google_apis/gaia/gaia_auth_util.h"

DiceWebSigninInterceptorDelegate::DiceWebSigninInterceptorDelegate() = default;

DiceWebSigninInterceptorDelegate::~DiceWebSigninInterceptorDelegate() = default;

std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble(
    content::WebContents* web_contents,
    const BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  if (!web_contents) {
    std::move(callback).Run(SigninInterceptionResult::kNotDisplayed);
    return nullptr;
  }
  return ShowSigninInterceptionBubbleInternal(
      chrome::FindBrowserWithWebContents(web_contents), bubble_parameters,
      std::move(callback));
}

void DiceWebSigninInterceptorDelegate::ShowProfileCustomizationBubble(
    Browser* browser) {
  ShowProfileCustomizationBubbleInternal(browser);
}

void DiceWebSigninInterceptorDelegate::ShowEnterpriseProfileInterceptionDialog(
    Browser* browser,
    const std::string& email,
    SkColor profile_color,
    base::OnceCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(kAccountPoliciesLoadedWithoutSync)) {
    browser->signin_view_controller()->ShowModalEnterpriseConfirmationDialog(
        gaia::ExtractDomainName(email), profile_color, std::move(callback));
    return;
  }

  DiceTurnSyncOnHelper::Delegate::ShowEnterpriseAccountConfirmationForBrowser(
      email, true,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             DiceTurnSyncOnHelper::SigninChoice choice) {
            std::move(callback).Run(
                choice == DiceTurnSyncOnHelper::SigninChoice::
                              SIGNIN_CHOICE_CONTINUE ||
                choice == DiceTurnSyncOnHelper::SigninChoice::
                              SIGNIN_CHOICE_NEW_PROFILE);
          },
          std::move(callback)),
      browser);
}
