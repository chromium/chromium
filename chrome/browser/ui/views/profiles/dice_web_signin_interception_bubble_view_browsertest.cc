// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

struct TestParam {
  std::string test_suffix = "";
  DiceWebSigninInterceptor::SigninInterceptionType interception_type =
      DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser;
  policy::EnterpriseManagementAuthority management_authority =
      policy::EnterpriseManagementAuthority::NONE;
  // Note: changes strings for kEnterprise type, otherwise adds badge on pic.
  bool is_intercepted_account_managed = false;
  bool use_dark_theme = false;
  SkColor4f intercepted_profile_color = SkColors::kLtGray;
  SkColor4f primary_profile_color = SkColors::kBlue;
};

class ProfileDestructionWatcher : public ProfileObserver {
 public:
  ProfileDestructionWatcher() = default;

  ProfileDestructionWatcher(const ProfileDestructionWatcher&) = delete;
  ProfileDestructionWatcher& operator=(const ProfileDestructionWatcher&) =
      delete;

  ~ProfileDestructionWatcher() override = default;

  void Watch(Profile* profile) { observed_profiles_.AddObservation(profile); }
  void WaitForDestruction() { run_loop_.Run(); }
  bool destroyed() const { return destroyed_; }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    DCHECK(!destroyed_) << "Double profile destruction";
    destroyed_ = true;
    observed_profiles_.RemoveObservation(profile);
    run_loop_.Quit();
  }

  bool destroyed_ = false;
  base::RunLoop run_loop_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `All/<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `kTestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported bubbles.
const TestParam kTestParams[] = {
    // Common consumer user case: regular account signing in to a profile having
    // a regular account on a non-managed device.
    {"ConsumerSimple",
     DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false,
     /*use_dark_theme=*/false,
     /*intercepted_profile_color=*/SkColors::kMagenta},

    // Ditto, with a different color scheme
    {"ConsumerDark",
     DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false,
     /*use_dark_theme=*/true,
     /*intercepted_profile_color=*/SkColors::kMagenta},

    // Regular account signing in to a profile having a regular account on a
    // managed device (having policies configured locally for example).
    {"ConsumerManagedDevice",
     DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
     policy::EnterpriseManagementAuthority::COMPUTER_LOCAL,
     /*is_intercepted_account_managed=*/false,
     /*use_dark_theme=*/false,
     /*intercepted_profile_color=*/SkColors::kYellow,
     /*primary_profile_color=*/SkColors::kMagenta},

    // Regular account signing in to a profile having a managed account on a
    // non-managed device.
    {"EnterpriseSimple",
     DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/false},

    // Managed account signing in to a profile having a regular account on a
    // non-managed device.
    {"EnterpriseManagedIntercepted",
     DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/true},

    // Ditto, with a different color scheme
    {"EnterpriseManagedInterceptedDark",
     DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::NONE,
     /*is_intercepted_account_managed=*/true,
     /*use_dark_theme=*/true},

    // Regular account signing in to a profile having a managed account on a
    // managed device.
    {"EntepriseManagedDevice",
     DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
     policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
     /*is_intercepted_account_managed=*/false},

    // Profile switch bubble: the account used for signing in is already
    // associated with another profile.
    {"ProfileSwitch",
     DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
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
  DiceWebSigninInterceptionBubblePixelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    // `kSigninInterceptBubbleV2` feature is tested in
    // `DiceWebSigninInterceptionBubbleV2PixelTest`
    std::vector<base::test::FeatureRef> disabled_features = {
        kSigninInterceptBubbleV2};
    if (GetParam().use_dark_theme) {
      enabled_features.push_back(features::kWebUIDarkMode);
    }
    base_scoped_feature_list_.InitWithFeatures(enabled_features,
                                               disabled_features);
  }

  // DialogBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        GetParam().management_authority);

    SkColor primary_highlight_color =
        GetParam().primary_profile_color.toSkColor();
    ProfileThemeColors colors = {
        /*profile_highlight_color=*/primary_highlight_color,
        /*default_avatar_fill_color=*/primary_highlight_color,
        /*default_avatar_stroke_color=*/
        GetAvatarStrokeColor(*browser()->window()->GetColorProvider(),
                             primary_highlight_color)};
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(browser()->profile()->GetPath());
    DCHECK(entry);
    entry->SetProfileThemeColors(colors);

    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIDiceWebSigninInterceptURL)};
    observer.StartWatchingNewWebContents();

    bubble_handle_ = DiceWebSigninInterceptionBubbleView::CreateBubble(
        browser(), GetAvatarButton(browser()), GetTestBubbleParameters(),
        base::DoNothing());

    observer.Wait();
  }

  // Generates bubble parameters for testing.
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
            GetParam().intercepted_profile_color.toSkColor()};
  }

  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> bubble_handle_;
  base::test::ScopedFeatureList base_scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubblePixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubblePixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

class DiceWebSigninInterceptionBubbleV2PixelTest
    : public DiceWebSigninInterceptionBubblePixelTest {
 public:
  DiceWebSigninInterceptionBubbleV2PixelTest() = default;

  base::test::ScopedFeatureList scoped_feature_list_{kSigninInterceptBubbleV2};
};

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubbleV2PixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubbleV2PixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

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
    return DiceWebSigninInterceptor::Delegate::BubbleParameters(
        DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser, account,
        primary_account);
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
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
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
  ProfileDestructionWatcher profile_destruction_watcher;
  profile_destruction_watcher.Watch(new_profile);
  new_browser->window()->Close();

  // The profile is not destroyed, because the bubble is retaining it.
  EXPECT_TRUE(g_browser_process->profile_manager()->HasKeepAliveForTesting(
      new_profile, ProfileKeepAliveOrigin::kDiceWebSigninInterceptionBubble));
  EXPECT_FALSE(profile_destruction_watcher.destroyed());

  // Close the bubble.
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  widget_destroyed_waiter.Wait();
  ASSERT_TRUE(callback_result_.has_value());
  EXPECT_EQ(callback_result_, SigninInterceptionResult::kIgnored);

  // The keep-alive is released and the profile is destroyed.
  profile_destruction_watcher.WaitForDestruction();

  // Check that histograms are recorded.
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                      SigninInterceptionResult::kIgnored, 1);
  histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser.NoSync",
                                      SigninInterceptionResult::kIgnored, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
}

class DiceWebSigninInterceptionBubbleV2BrowserTest
    : public DiceWebSigninInterceptionBubbleBrowserTest {
 public:
  DiceWebSigninInterceptionBubbleV2BrowserTest() = default;

  DiceWebSigninInterceptor::Delegate::BubbleParameters
  GetTestBubbleParametersForManagedProfile() {
    DiceWebSigninInterceptor::Delegate::BubbleParameters bubble_parameters =
        GetTestBubbleParameters();
    bubble_parameters.show_managed_disclaimer = true;
    return bubble_parameters;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{kSigninInterceptBubbleV2};
};

// Tests that clicking the Learn More link in the bubble opens the page in a new
// tab.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleV2BrowserTest,
                       OpenLearnMoreLinkInNewTab) {
  const GURL bubble_url("chrome://signin-dice-web-intercept/");
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
