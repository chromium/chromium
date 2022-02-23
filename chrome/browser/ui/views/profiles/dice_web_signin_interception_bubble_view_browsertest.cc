// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

struct TestParam {
  DiceWebSigninInterceptor::SigninInterceptionType interception_type =
      DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser;
  policy::EnterpriseManagementAuthority management_authority =
      policy::EnterpriseManagementAuthority::NONE;
  // Note: changes strings for kEnterprise type, otherwise adds badge on pic.
  bool is_intercepted_account_managed = false;
};

// Permutations of supported bubbles.
const TestParam kTestParams[] = {
    // Common consumer user case: regular account signing in to a profile having
    // a regular account on a non-managed device.
    {DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false},

    // Regular account signing in to a profile having a regular account on a
    // managed device (having policies configured locally for example).
    {DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
     policy::EnterpriseManagementAuthority::COMPUTER_LOCAL,
     /*is_intercepted_account_managed=*/false},

    // Regular account signing in to a profile having a managed account on a
    // non-managed device.
    {DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false},

    // Managed account signing in to a profile having a regular account on a
    // non-managed device.
    {DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/true},

    // Regular account signing in to a profile having a managed account on a
    // managed device.
    {DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
     /*is_intercepted_account_managed=*/false},

    // Profile switch bubble: the account used for signing in is already
    // associated with another profile.
    {DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false},
};

// Returns the avatar button, which is the anchor view for the interception
// bubble.
views::View* GetAvatarButton(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* avatar_button =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  DCHECK(avatar_button);
  return avatar_button;
}

}  // namespace

class DiceWebSigninInterceptionBubblePixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  DiceWebSigninInterceptionBubblePixelTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        GetParam().management_authority);

    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIDiceWebSigninInterceptURL)};
    observer.StartWatchingNewWebContents();

    bubble_handle_ = DiceWebSigninInterceptionBubbleView::CreateBubble(
        browser()->profile(), GetAvatarButton(browser()),
        GetTestBubbleParameters(), base::DoNothing());

    observer.Wait();
  }

  // Returns dummy bubble parameters for testing.
  DiceWebSigninInterceptor::Delegate::BubbleParameters
  GetTestBubbleParameters() {
    AccountInfo intercepted_account;
    intercepted_account.account_id =
        CoreAccountId::FromGaiaId("intercepted_ID");
    intercepted_account.given_name = "Sam";
    intercepted_account.full_name = "Sam Sample";
    intercepted_account.email = "sam.sample@intercepted.com";
    intercepted_account.hosted_domain =
        GetParam().is_intercepted_account_managed ? "intercepted.com"
                                                  : kNoHostedDomainFound;

    // `kEnterprise` type bubbles are used when at least one of the accounts is
    // managed. Instead of explicitly specifying it in the test parameters, we
    // can infer whether the primary account should be managed based on this,
    // since no test config has both accounts being managed.
    bool is_primary_account_managed =
        GetParam().interception_type ==
            DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise &&
        !GetParam().is_intercepted_account_managed;
    AccountInfo primary_account;
    primary_account.account_id = CoreAccountId::FromGaiaId("primary_ID");
    primary_account.given_name = "Tessa";
    primary_account.full_name = "Tessa Tester";
    primary_account.email = "tessa.tester@primary.com";
    primary_account.hosted_domain =
        is_primary_account_managed ? "primary.com" : kNoHostedDomainFound;

    return {GetParam().interception_type, intercepted_account, primary_account,
            SkColors::kLtGray.toSkColor()};
  }

  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> bubble_handle_;
};

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubblePixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubblePixelTest,
                         testing::ValuesIn(kTestParams));

class DiceWebSigninInterceptionBubbleSyncPromoPixelTest
    : public DiceWebSigninInterceptionBubblePixelTest {
 public:
  DiceWebSigninInterceptionBubbleSyncPromoPixelTest() = default;

  base::test::ScopedFeatureList scoped_feature_list_{
      kSyncPromoAfterSigninIntercept};
};

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubbleSyncPromoPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubbleSyncPromoPixelTest,
                         testing::ValuesIn(kTestParams));

class DiceWebSigninInterceptionBubbleBrowserTest : public InProcessBrowserTest {
 public:
  DiceWebSigninInterceptionBubbleBrowserTest() = default;

  views::View* GetAvatarButton() { return ::GetAvatarButton(browser()); }

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

  absl::optional<SigninInterceptionResult> callback_result_;
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> bubble_handle_;
};

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

  views::test::WidgetDestroyedWaiter closing_observer(widget);
  EXPECT_FALSE(bubble->GetAccepted());
  // Simulate clicking Accept in the WebUI.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kAccept);
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kAccepted);
  EXPECT_TRUE(bubble->GetAccepted());

  // Widget was not closed yet.
  ASSERT_FALSE(widget->IsClosed());
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
