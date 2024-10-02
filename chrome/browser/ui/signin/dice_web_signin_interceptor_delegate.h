// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/signin/web_signin_interceptor.h"

namespace content {
class WebContents;
}

class Browser;
class Profile;
struct CoreAccountId;

class DiceWebSigninInterceptorDelegate : public WebSigninInterceptor::Delegate {
 public:
  DiceWebSigninInterceptorDelegate();
  ~DiceWebSigninInterceptorDelegate() override;

  // DiceWebSigninInterceptor::Delegate
  bool IsSigninInterceptionSupported(
      const content::WebContents& web_contents) override;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowOidcInterceptionDialog(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      signin::SigninChoiceWithConfirmationCallback callback,
      base::OnceClosure dialog_closed_closure,
      base::OnceClosure retry_callback = base::DoNothing()) override;
  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override;

  // Returns the histogram suffix related to the given interception type.
  static std::string GetHistogramSuffix(
      WebSigninInterceptor::SigninInterceptionType interception_type);

  // Record metrics about the result of the signin interception.
  static void RecordInterceptionResult(
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      Profile* profile,
      SigninInterceptionResult result);

 private:
  // Implemented in dice_web_signin_interception_bubble_view.cc
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubbleInternal(
      Browser* browser,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Implemented in profile_customization_bubble_view.cc
  static bool IsSigninInterceptionSupportedInternal(const Browser& Browser);
  void ShowProfileCustomizationBubbleInternal(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
