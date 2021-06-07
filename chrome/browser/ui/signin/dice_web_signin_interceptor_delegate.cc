// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

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
    const std::string& email,
    base::OnceCallback<void(bool)> callback,
    Browser* browser) {
  // TODO (crbug/1163117): Replace this temporary solution with the spaces
  // enterprise welcome screen inside a dialog.
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
