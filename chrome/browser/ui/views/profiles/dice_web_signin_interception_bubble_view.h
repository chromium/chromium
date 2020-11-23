// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"

namespace views {
class View;
}  // namespace views

class Profile;

// Bubble shown as part of Dice web signin interception. This bubble is
// implemented as a WebUI page rendered inside a native bubble.
class DiceWebSigninInterceptionBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  ~DiceWebSigninInterceptionBubbleView() override;

  DiceWebSigninInterceptionBubbleView(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  DiceWebSigninInterceptionBubbleView& operator=(
      const DiceWebSigninInterceptionBubbleView& other) = delete;

  static void CreateBubble(
      Profile* profile,
      views::View* anchor_view,
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Record metrics about the result of the signin interception.
  static void RecordInterceptionResult(
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      Profile* profile,
      SigninInterceptionResult result);

 private:
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleClosed);

  DiceWebSigninInterceptionBubbleView(
      Profile* profile,
      views::View* anchor_view,
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // This bubble has no native buttons. The user accepts or cancels through this
  // method, which is called by the inner web UI.
  void OnWebUIUserChoice(bool accept);

  Profile* profile_;
  DiceWebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
