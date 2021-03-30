// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class DiceWebSigninInterceptionBubbleBrowserTest : public DialogBrowserTest {
 public:
  DiceWebSigninInterceptionBubbleBrowserTest() {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, /*enabled=*/true);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    bubble_handle_ = DiceWebSigninInterceptionBubbleView::CreateBubble(
        browser()->profile(), GetAvatarButton(), GetTestBubbleParameters(),
        base::DoNothing());
  }

  // Returns the avatar button, which is the anchor view for the interception
  // bubble.
  views::View* GetAvatarButton() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(avatar_button);
    return avatar_button;
  }

  // Completion callback for the interception bubble.
  void OnInterceptionComplete(SigninInterceptionResult result) {
    DCHECK(!callback_result_.has_value());
    callback_result_ = result;
  }

  // Returns dummy bubble parameters for testing.
  DiceWebSigninInterceptor::Delegate::BubbleParameters
  GetTestBubbleParameters() {
    AccountInfo account;
    account.account_id = CoreAccountId::FromGaiaId("ID1");
    AccountInfo primary_account;
    primary_account.account_id = CoreAccountId::FromGaiaId("ID2");
    return {DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
            account, primary_account};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Optional<SigninInterceptionResult> callback_result_;
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> bubble_handle_;
};

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that the callback is called once when the bubble is closed.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleClosed) {
  base::HistogramTester histogram_tester;
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      new DiceWebSigninInterceptionBubbleView(
          browser()->profile(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this))));
  widget->Show();
  EXPECT_FALSE(callback_result_.has_value());

  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  waiter.Wait();
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kIgnored);

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kIgnored, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kIgnored, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
}

// Tests that the callback is called once when the bubble is declined.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleDeclined) {
  base::HistogramTester histogram_tester;
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser()->profile(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  EXPECT_FALSE(callback_result_.has_value());

  views::test::WidgetDestroyedWaiter waiter(widget);
  // Simulate clicking Cancel in the WebUI.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kDecline);
  waiter.Wait();
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kDeclined);

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kDeclined, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
}

// Tests that the callback is called once when the bubble is accepted. The
// bubble is not destroyed until a new browser window is created.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleAccepted) {
  base::HistogramTester histogram_tester;
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser()->profile(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  EXPECT_FALSE(callback_result_.has_value());

  // Take a handle on the bubble, to close it later.
  bubble_handle_ = bubble->GetHandle();

  views::test::WidgetClosingObserver closing_observer(widget);
  EXPECT_FALSE(bubble->GetAccepted());
  // Simulate clicking Accept in the WebUI.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kAccept);
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kAccepted);
  EXPECT_TRUE(bubble->GetAccepted());

  // Widget was not closed yet.
  ASSERT_FALSE(closing_observer.widget_closed());
  // Simulate completion of the interception process.
  bubble_handle_.reset();
  // Widget will close now.
  closing_observer.Wait();

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kAccepted, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
}

// Tests that the callback is called once when the bubble is accepted with Guest
// mode offer. The bubble is not destroyed until a new browser window is
// created.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleAcceptedGuestMode) {
  if (!Profile::IsEphemeralGuestProfileEnabled())
    return;
  base::HistogramTester histogram_tester;
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser()->profile(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  EXPECT_FALSE(callback_result_.has_value());

  // Take a handle on the bubble, to close it later.
  bubble_handle_ = bubble->GetHandle();

  views::test::WidgetClosingObserver closing_observer(widget);
  EXPECT_FALSE(bubble->GetAccepted());
  // Simulate clicking Guest in the WebUI.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kGuest);
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kAcceptedWithGuest);
  EXPECT_TRUE(bubble->GetAccepted());

  // Widget was not closed yet.
  ASSERT_FALSE(closing_observer.widget_closed());
  // Simulate completion of the interception process.
  bubble_handle_.reset();
  // Widget will close now.
  closing_observer.Wait();

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample(
      "Signin.InterceptResult.MultiUser",
      SigninInterceptionResult::kAcceptedWithGuest, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.InterceptResult.MultiUser.NoSync",
      SigninInterceptionResult::kAcceptedWithGuest, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
}
