// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

class Profile;

// Bubble shown as part of Dice web signin interception. This bubble is
// implemented as a WebUI page rendered inside a native bubble.
class DiceWebSigninInterceptionBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(DiceWebSigninInterceptionBubbleView);
  DiceWebSigninInterceptionBubbleView(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  DiceWebSigninInterceptionBubbleView& operator=(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  ~DiceWebSigninInterceptionBubbleView() override;

  // Warning: the bubble is closed when the handle is destroyed ; it is the
  // responsibility of the caller to keep the handle alive until the bubble
  // should be closed.
  static std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
  CreateBubble(Profile* profile,
               views::View* anchor_view,
               const DiceWebSigninInterceptor::Delegate::BubbleParameters&
                   bubble_parameters,
               base::OnceCallback<void(SigninInterceptionResult)> callback)
      WARN_UNUSED_RESULT;

  // Record metrics about the result of the signin interception.
  static void RecordInterceptionResult(
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      Profile* profile,
      SigninInterceptionResult result);

  // Returns true if the user has accepted the interception.
  bool GetAccepted() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleClosed);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleDeclined);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleAccepted);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleAcceptedGuestMode);
  FRIEND_TEST_ALL_PREFIXES(ProfileBubbleInteractiveUiTest,
                           InterceptionBubbleFocus);

  // Closes the bubble when `ScopedHandle` is destroyed. Does nothing if the
  // bubble has been already closed.
  class ScopedHandle : public ScopedDiceWebSigninInterceptionBubbleHandle {
   public:
    explicit ScopedHandle(
        base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble);
    ~ScopedHandle() override;

    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(const ScopedHandle&) = delete;

   private:
    base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble_;
  };

  DiceWebSigninInterceptionBubbleView(
      Profile* profile,
      views::View* anchor_view,
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Gets a handle on the bubble. Warning: the bubble is closed when the handle
  // is destroyed ; it is the responsibility of the caller to keep the handle
  // alive until the bubble should be closed.
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> GetHandle()
      const;

  // This bubble has no native buttons. The user accepts or cancels or selects
  // Guest profile through this method, which is called by the inner web UI.
  void OnWebUIUserChoice(SigninInterceptionUserChoice user_choice);

  Profile* profile_;
  bool accepted_ = false;
  DiceWebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;

  // Last member in the class: pointers are invalidated before other fields.
  base::WeakPtrFactory<DiceWebSigninInterceptionBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
