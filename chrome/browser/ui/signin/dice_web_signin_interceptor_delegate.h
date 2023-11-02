// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include "base/callback_forward.h"

namespace content {
class WebContents;
}

class Browser;
struct CoreAccountId;

class DiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  DiceWebSigninInterceptorDelegate();
  ~DiceWebSigninInterceptorDelegate() override;

  // DiceWebSigninInterceptor::Delegate
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override;
  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      DiceWebSigninInterceptor::SigninInterceptionType interception_type)
      override;

 private:
  // Implemented in dice_web_signin_interception_bubble_view.cc
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubbleInternal(
      Browser* browser,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Implemented in profile_customization_bubble_view.cc
  void ShowProfileCustomizationBubbleInternal(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
