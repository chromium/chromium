// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_

#include "base/functional/callback_helpers.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
class WebView;
}  // namespace views

class Browser;
class Profile;

// Bubble shown as part of Dice web signin interception. This bubble is
// implemented as a WebUI page rendered inside a native bubble.
class DiceWebSigninInterceptionBubbleView
    : public views::BubbleDialogDelegateView,
      content::WebContentsDelegate {
  METADATA_HEADER(DiceWebSigninInterceptionBubbleView,
                  views::BubbleDialogDelegateView)

 public:
  DiceWebSigninInterceptionBubbleView(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  DiceWebSigninInterceptionBubbleView& operator=(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  ~DiceWebSigninInterceptionBubbleView() override;

  // Warning: the bubble is closed when the handle is destroyed ; it is the
  // responsibility of the caller to keep the handle alive until the bubble
  // should be closed.
  [[nodiscard]] static std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  CreateBubble(
      Browser* browser,
      views::View* anchor_view,
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Record metrics about the result of the signin interception.
  static void RecordInterceptionResult(
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      Profile* profile,
      SigninInterceptionResult result);

  // Returns true if the user has accepted the interception.
  bool GetAccepted() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleIgnored);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleDeclined);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleAccepted);
  FRIEND_TEST_ALL_PREFIXES(
      DiceWebSigninInterceptionBubbleWithExplicitBrowserSigninBrowserTest,
      BubbleDismissedByEscapeKey);
  FRIEND_TEST_ALL_PREFIXES(
      DiceWebSigninInterceptionBubbleWithExplicitBrowserSigninBrowserTest,
      BubbleDismissedByPressingAvatarButton);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleAcceptedGuestMode);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           ProfileKeepAlive);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           OpenLearnMoreLinkInNewTab);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           ChromeSigninAccepted);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           ChromeSigninDeclined);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleWithParamBrowserTest,
                           AvatarEffectWithInterceptType);
  FRIEND_TEST_ALL_PREFIXES(ProfileBubbleInteractiveUiTest,
                           InterceptionBubbleFocus);

  // Closes the bubble when `ScopedHandle` is destroyed. Does nothing if the
  // bubble has been already closed.
  class ScopedHandle : public ScopedWebSigninInterceptionBubbleHandle {
   public:
    explicit ScopedHandle(
        base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble);
    ~ScopedHandle() override;

    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(const ScopedHandle&) = delete;

    DiceWebSigninInterceptionBubbleView* GetBubbleViewForTesting() {
      return bubble_.get();
    }

   private:
    base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble_;
  };

  DiceWebSigninInterceptionBubbleView(
      Browser* browser,
      views::View* anchor_view,
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Gets a handle on the bubble. Warning: the bubble is closed when the handle
  // is destroyed ; it is the responsibility of the caller to keep the handle
  // alive until the bubble should be closed.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> GetHandle();

  // This bubble has no native buttons. The user accepts or cancels, which is
  // called by the inner web UI.
  void OnWebUIUserChoice(SigninInterceptionUserChoice user_choice);

  // Treats the response based on the interception result.
  void OnInterceptionResult(SigninInterceptionResult result);

  content::WebContents* GetBubbleWebContentsForTesting();

  // Callback to set the final height of the bubble based on its content, after
  // the page is loaded and the height is sent by DiceWebSigninInterceptHandler.
  void SetHeightAndShowWidget(int height);

  // content::WebContentsDelegate:
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // Dimisses the bubbles and finishes the flow.
  void Dismiss(SigninInterceptionDismissReason reason);

  // Returns whether the bubble displayed is the Chrome Signin one or not.
  bool IsChromeSignin() const;

  // Applies the effect of the changing the text and the button action of the
  // identity pill avatar button.
  void ApplyAvatarButtonEffects();
  // Resets the effect applied in `ApplyAvatarButtonEffects()`.
  void ClearAvatarButtonEffects();

  // This bubble can outlive the Browser, in particular on Mac (see
  // https://crbug.com/1302729). Retain the profile to prevent use-after-free.
  ScopedProfileKeepAlive profile_keep_alive_;

  base::WeakPtr<Browser> browser_;
  raw_ptr<Profile> profile_;
  bool accepted_ = false;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;
  raw_ptr<views::WebView> web_view_;

  base::TimeTicks chrome_signin_bubble_shown_time_;

  base::ScopedClosureRunner hide_avatar_text_callback_;
  base::ScopedClosureRunner reset_avatar_button_action_callback_;

  // Last member in the class: pointers are invalidated before other fields.
  base::WeakPtrFactory<DiceWebSigninInterceptionBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
