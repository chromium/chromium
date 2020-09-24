// Copyright 2020 The Chromium Authors. All rights reserved.
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

class DiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  DiceWebSigninInterceptorDelegate();
  ~DiceWebSigninInterceptorDelegate() override;

  // DiceWebSigninInterceptor::Delegate
  void ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(bool)> callback) override;
  void ShowProfileCustomizationBubble(Browser* browser) override;

 private:
  // Implemented in dice_web_signin_interception_bubble_view.cc
  void ShowSigninInterceptionBubbleInternal(
      Browser* browser,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(bool)> callback);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
