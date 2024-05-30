// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the avatar button, which is the anchor view for the interception
// bubble.
AvatarToolbarButton* GetAvatarButton(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  AvatarToolbarButton* avatar_button =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  DCHECK(avatar_button);
  return avatar_button;
}

// Unable to use `content::SimulateKeyPress()` helper function since it sets
// `event.skip_if_unhandled` to true which stops the propagation of the event to
// the delegate web view.
void SimulateEscapeKeyPress(content::WebContents* web_content) {
  // Create the escape key press event.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  event.dom_key = ui::DomKey::ESCAPE;
  event.dom_code = static_cast<int>(ui::DomCode::ESCAPE);

  // Send the event to the Web Contents.
  web_content->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
}

}  // namespace

class DiceWebSigninInterceptionBubbleBrowserTest : public InProcessBrowserTest {
 public:
  DiceWebSigninInterceptionBubbleBrowserTest() = default;

  AvatarToolbarButton* GetAvatarButton() {
    return ::GetAvatarButton(browser());
  }

  // Completion callback for the interception bubble.
  void OnInterceptionComplete(SigninInterceptionResult result) {
    DCHECK(!callback_result_.has_value());
    callback_result_ = result;
  }

  // Returns bubble parameters for testing.
  WebSigninInterceptor::Delegate::BubbleParameters
  GetTestBubbleParametersWithInterceptType(
      WebSigninInterceptor::SigninInterceptionType intercept_type) {
    AccountInfo account;
    account.account_id = CoreAccountId::FromGaiaId("ID1");
    AccountInfo primary_account;
    if (intercept_type !=
        WebSigninInterceptor::SigninInterceptionType::kChromeSignin) {
      primary_account.account_id = CoreAccountId::FromGaiaId("ID2");
    }
    return WebSigninInterceptor::Delegate::BubbleParameters(
        intercept_type, account, primary_account);
  }

  // Returns dummy bubble parameters for testing.
  WebSigninInterceptor::Delegate::BubbleParameters GetTestBubbleParameters() {
    return GetTestBubbleParametersWithInterceptType(
        WebSigninInterceptor::SigninInterceptionType::kMultiUser);
  }

  // Returns bubble parameters for Chrome Signin bubble testing.
  WebSigninInterceptor::Delegate::BubbleParameters
  GetTestChromeSigninBubbleParameters() {
    return GetTestBubbleParametersWithInterceptType(
        WebSigninInterceptor::SigninInterceptionType::kChromeSignin);
  }

  WebSigninInterceptor::Delegate::BubbleParameters
  GetTestBubbleParametersForManagedProfile() {
    WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters =
        GetTestBubbleParameters();
    bubble_parameters.show_managed_disclaimer = true;
    return bubble_parameters;
  }

  std::optional<SigninInterceptionResult> callback_result_;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> bubble_handle_;
};

// Tests that the callback is called once when the bubble is ignored.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleIgnored) {
  base::HistogramTester histogram_tester;
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      new DiceWebSigninInterceptionBubbleView(
          browser(), GetAvatarButton(), GetTestBubbleParameters(),
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
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));
}

class DiceWebSigninInterceptionBubbleWithExplicitBrowserSigninBrowserTest
    : public DiceWebSigninInterceptionBubbleBrowserTest {
 private:
  // Activates some new UI behaviors around dismissing the bubles.
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// Tests that the callback is called once when the bubble is dismissed.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptionBubbleWithExplicitBrowserSigninBrowserTest,
    BubbleDismissedByEscapeKey) {
  base::HistogramTester histogram_tester;
  // Creating the bubble through the static function.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      DiceWebSigninInterceptionBubbleView::CreateBubble(
          browser(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));

  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      static_cast<DiceWebSigninInterceptionBubbleView::ScopedHandle*>(
          handle.get())
          ->GetBubbleViewForTesting();

  views::Widget* widget = bubble->GetWidget();
  views::test::WidgetVisibleWaiter visible_waiter(widget);
  visible_waiter.Wait();
  EXPECT_FALSE(callback_result_.has_value());

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  // Pressing the escape key should dismiss the bubble.
  SimulateEscapeKeyPress(bubble->GetBubbleWebContentsForTesting());
  destroyed_waiter.Wait();
  EXPECT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kDismissed);

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kDismissed, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kDismissed, 1);
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));

  // Dismiss reason histograms.
  histogram_tester.ExpectUniqueSample("Signin.Intercept.BubbleDismissReason",
                                      SigninInterceptionDismissReason::kEscKey,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.BubbleDismissReason.MultiUser",
      SigninInterceptionDismissReason::kEscKey, 1);
}

// Same as the above test, but dismissing by pressing the avatar button.
// Only difference in expectations is the histograms records.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptionBubbleWithExplicitBrowserSigninBrowserTest,
    BubbleDismissedByPressingAvatarButton) {
  base::HistogramTester histogram_tester;
  // Creating the bubble through the static function.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      DiceWebSigninInterceptionBubbleView::CreateBubble(
          browser(), GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));

  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      static_cast<DiceWebSigninInterceptionBubbleView::ScopedHandle*>(
          handle.get())
          ->GetBubbleViewForTesting();

  views::Widget* widget = bubble->GetWidget();
  views::test::WidgetVisibleWaiter visible_waiter(widget);
  visible_waiter.Wait();
  EXPECT_FALSE(callback_result_.has_value());

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  // Pressing the avatar button should dismiss the bubble.
  GetAvatarButton()->ButtonPressed();
  destroyed_waiter.Wait();
  EXPECT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kDismissed);

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kDismissed, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kDismissed, 1);
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));

  // Dismiss reason histograms.
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.BubbleDismissReason",
      SigninInterceptionDismissReason::kIdentityPillPressed, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.BubbleDismissReason.MultiUser",
      SigninInterceptionDismissReason::kIdentityPillPressed, 1);
}

// Tests that the callback is called once when the bubble is declined.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleDeclined) {
  base::HistogramTester histogram_tester;
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser(), GetAvatarButton(), GetTestBubbleParameters(),
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
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));
}

// Tests that the callback is called once when the bubble is accepted. The
// bubble is not destroyed until a new browser window is created.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       BubbleAccepted) {
  base::HistogramTester histogram_tester;
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser(), GetAvatarButton(), GetTestBubbleParameters(),
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
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));
}

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       ProfileKeepAlive) {
  base::HistogramTester histogram_tester;

  // Create a temporary profile with a new browser.
  Profile* new_profile = nullptr;
  base::RunLoop run_loop;
  ProfileManager::CreateMultiProfileAsync(
      u"test_profile", /*icon_index=*/0, /*is_hidden=*/false,
      base::BindLambdaForTesting([&new_profile, &run_loop](Profile* profile) {
        ASSERT_TRUE(profile);
        new_profile = profile;
        run_loop.Quit();
      }));
  run_loop.Run();
  Browser::CreateParams browser_params(new_profile, /*user_gesture=*/true);
  Browser* new_browser = Browser::Create(browser_params);
  new_browser->window()->Show();

  // Create a bubble using the temporary profile, but not attached to its view
  // hierarchy. This bubble won't be destroyed when the new browser is closed,
  // and will outlive it.
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      new DiceWebSigninInterceptionBubbleView(
          new_browser, GetAvatarButton(), GetTestBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this))));
  widget->Show();
  EXPECT_FALSE(callback_result_.has_value());

  // Close the browser without closing the bubble.
  ProfileDestructionWaiter profile_destruction_waiter(new_profile);
  new_browser->window()->Close();

  // The profile is not destroyed, because the bubble is retaining it.
  EXPECT_TRUE(g_browser_process->profile_manager()->HasKeepAliveForTesting(
      new_profile, ProfileKeepAliveOrigin::kDiceWebSigninInterceptionBubble));
  EXPECT_FALSE(profile_destruction_waiter.destroyed());

  // Close the bubble.
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  widget_destroyed_waiter.Wait();
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kIgnored);

  // The keep-alive is released and the profile is destroyed.
  profile_destruction_waiter.Wait();

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kIgnored, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kIgnored, 1);
  // Make sure no other histograms are recorded.
  base::HistogramTester::CountsMap expected_histogram_total_count = {
      {"Signin.InterceptResult.MultiUser", 1},
      {"Signin.InterceptResult.MultiUser.NoSync", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult."),
      testing::ContainerEq(expected_histogram_total_count));
}

// Tests that clicking the Learn More link in the bubble opens the page in a new
// tab.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       OpenLearnMoreLinkInNewTab) {
  const GURL bubble_url(chrome::kChromeUIDiceWebSigninInterceptURL);
  const GURL learn_more_url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kSigninInterceptManagedDisclaimerLearnMoreURL),
      g_browser_process->GetApplicationLocale());
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(
          browser(), GetAvatarButton(),
          GetTestBubbleParametersForManagedProfile(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();

  content::WebContents* bubble_web_contents =
      bubble->GetBubbleWebContentsForTesting();
  DCHECK(bubble_web_contents);
  content::WaitForLoadStop(bubble_web_contents);
  EXPECT_EQ(bubble_web_contents->GetVisibleURL(), bubble_url);

  content::TestNavigationObserver new_tab_observer(nullptr);
  new_tab_observer.StartWatchingNewWebContents();

  ASSERT_TRUE(content::ExecJs(
      bubble_web_contents,
      "document.querySelector('dice-web-signin-intercept-app').shadowRoot."
      "querySelector('#managedDisclaimerText a').click();"));
  new_tab_observer.Wait();

  content::WebContents* new_tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_tab_web_contents, bubble_web_contents);
  EXPECT_EQ(new_tab_web_contents->GetVisibleURL(), learn_more_url);
  EXPECT_FALSE(widget->IsClosed());
}

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       ChromeSigninAccepted) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ASSERT_FALSE(GetAvatarButton()->IsButtonActionDisabled());
  // Creating the bubble through the static function.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      DiceWebSigninInterceptionBubbleView::CreateBubble(
          browser(), GetAvatarButton(), GetTestChromeSigninBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      static_cast<DiceWebSigninInterceptionBubbleView::ScopedHandle*>(
          handle.get())
          ->GetBubbleViewForTesting();

  views::Widget* widget = bubble->GetWidget();
  // Equivalent to `kInterceptionBubbleBaseHeight` default.
  bubble->SetHeightAndShowWidget(/*height=*/500);
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
  EXPECT_FALSE(GetAvatarButton()->IsButtonActionDisabled());

  // Widget was not closed yet - the delegate then takes care of it through the
  // handle.
  ASSERT_FALSE(widget->IsClosed());
  // Simulate completion of the interception process.
  bubble_handle_.reset();
  // Widget will close now.
  closing_observer.Wait();

  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.ChromeSignin",
                                      SigninInterceptionResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE,
      1);
  base::HistogramTester::CountsMap expected_time_histogram_total_count = {
      {"Signin.Intercept.ChromeSignin.ResponseTimeAccepted", 1},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Signin.Intercept.ChromeSignin.ResponseTime"),
              testing::ContainerEq(expected_time_histogram_total_count));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Signin_FromChromeSigninInterceptBubble"));
}

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       ChromeSigninDeclined) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(GetAvatarButton()->GetEnabled());
  // Creating the bubble through the static function.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      DiceWebSigninInterceptionBubbleView::CreateBubble(
          browser(), GetAvatarButton(), GetTestChromeSigninBubbleParameters(),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));
  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      static_cast<DiceWebSigninInterceptionBubbleView::ScopedHandle*>(
          handle.get())
          ->GetBubbleViewForTesting();

  views::Widget* widget = bubble->GetWidget();
  // Equivalent to `kInterceptionBubbleBaseHeight` default.
  bubble->SetHeightAndShowWidget(/*height=*/500);
  EXPECT_FALSE(callback_result_.has_value());

  views::test::WidgetDestroyedWaiter closing_observer(widget);
  EXPECT_FALSE(bubble->GetAccepted());
  // Simulate clicking Decline in the WebUI.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kDecline);
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kDeclined);
  EXPECT_FALSE(bubble->GetAccepted());
  EXPECT_FALSE(GetAvatarButton()->IsButtonActionDisabled());

  EXPECT_TRUE(widget->IsClosed());
  // Widget will close now.
  closing_observer.Wait();

  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.ChromeSignin",
                                      SigninInterceptionResult::kDeclined, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE,
      1);
  base::HistogramTester::CountsMap expected_time_histogram_total_count = {
      {"Signin.Intercept.ChromeSignin.ResponseTimeDeclined", 1},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Signin.Intercept.ChromeSignin.ResponseTime"),
              testing::ContainerEq(expected_time_histogram_total_count));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Signin_FromChromeSigninInterceptBubble"));
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
struct InterceptTypesParam {
  WebSigninInterceptor::SigninInterceptionType intercept_type;
  int expected_avatar_text_id;
};

const InterceptTypesParam kInterceptTypesTestParams[] = {
    {.intercept_type = WebSigninInterceptor::SigninInterceptionType::kMultiUser,
     .expected_avatar_text_id =
         IDS_SIGNIN_DICE_WEB_INTERCEPT_AVATAR_BUTTON_SEPARATE_BROWSING_TEXT},
    {.intercept_type =
         WebSigninInterceptor::SigninInterceptionType::kEnterprise,
     .expected_avatar_text_id =
         IDS_SIGNIN_DICE_WEB_INTERCEPT_AVATAR_BUTTON_SEPARATE_BROWSING_TEXT},
    {.intercept_type =
         WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
     .expected_avatar_text_id =
         IDS_SIGNIN_DICE_WEB_INTERCEPT_AVATAR_BUTTON_SWITCH_PROFILE_TEXT},
    {.intercept_type =
         WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
     .expected_avatar_text_id =
         IDS_AVATAR_BUTTON_INTERCEPT_BUBBLE_CHROME_SIGNIN_TEXT},
};

class DiceWebSigninInterceptionBubbleWithParamBrowserTest
    : public DiceWebSigninInterceptionBubbleBrowserTest,
      public testing::WithParamInterface<InterceptTypesParam> {
 public:
  WebSigninInterceptor::SigninInterceptionType intercept_type() {
    return GetParam().intercept_type;
  }

  std::u16string expected_avatar_text() {
    return l10n_util::GetStringUTF16(GetParam().expected_avatar_text_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubbleWithParamBrowserTest,
                       AvatarEffectWithInterceptType) {
  AvatarToolbarButton* avatar_button = GetAvatarButton();
  ASSERT_FALSE(avatar_button->HasExplicitButtonAction());
  ASSERT_TRUE(avatar_button->GetText().empty());

  // Creating the bubble through the static function.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      DiceWebSigninInterceptionBubbleView::CreateBubble(
          browser(), avatar_button,
          GetTestBubbleParametersWithInterceptType(intercept_type()),
          base::BindOnce(&DiceWebSigninInterceptionBubbleBrowserTest::
                             OnInterceptionComplete,
                         base::Unretained(this)));

  // `bubble` is owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      static_cast<DiceWebSigninInterceptionBubbleView::ScopedHandle*>(
          handle.get())
          ->GetBubbleViewForTesting();
  // Equivalent to `kInterceptionBubbleBaseHeight` default.
  bubble->SetHeightAndShowWidget(/*height=*/500);

  EXPECT_TRUE(avatar_button->HasExplicitButtonAction());
  EXPECT_EQ(avatar_button->GetText(), expected_avatar_text());

  views::Widget* widget = bubble->GetWidget();

  views::test::WidgetDestroyedWaiter closing_observer(widget);
  // Simulating declining the bubble.
  bubble->OnWebUIUserChoice(SigninInterceptionUserChoice::kDecline);

  EXPECT_TRUE(widget->IsClosed());
  // Widget will close now.
  closing_observer.Wait();

  EXPECT_FALSE(avatar_button->HasExplicitButtonAction());
  EXPECT_TRUE(avatar_button->GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(,
                         DiceWebSigninInterceptionBubbleWithParamBrowserTest,
                         testing::ValuesIn(kInterceptTypesTestParams));
#endif
