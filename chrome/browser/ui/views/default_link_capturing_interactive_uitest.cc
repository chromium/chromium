// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"
#endif  // BUILDFLAG(IS_MAC)

// This test only runs on Windows, Mac and Linux platforms.
// Because some tests here rely on browser activation, an
// interactive_ui_test is preferred over a browser test.
class DefaultLinkCapturingInteractiveUiTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  DefaultLinkCapturingInteractiveUiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(), {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  size_t GetItemContainerSize(IntentPickerBubbleView* bubble) {
    return bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

  GURL GetOuterUrl() {
    return embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  }

  GURL GetInnerNestedUrl() {
    return embedded_test_server()->GetURL(
        "/web_apps/nesting/nested/index.html");
  }

  GURL GetNestedPageUrl() {
    return embedded_test_server()->GetURL(
        "/web_apps/nesting/nested/page1.html");
  }

  // Returns [outer app_id, inner app_id]
  std::tuple<webapps::AppId, webapps::AppId> InstallOuterAppAndInnerApp() {
    // The inner app must be installed first so that it is installable.
    webapps::AppId inner_app_id =
        web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(),
                                                         GetInnerNestedUrl());
    webapps::AppId outer_app_id =
        web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(),
                                                         GetOuterUrl());
    return {outer_app_id, inner_app_id};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultLinkCapturingInteractiveUiTest,
                       IntentPickerBubbleAcceptCorrectActions) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  EXPECT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));

  base::UserActionTester user_action_tester;
  // The app list is currently not deterministically ordered, so find the
  // correct item and select that.
  const auto& app_infos =
      web_app::intent_picker_bubble()->app_info_for_testing();  // IN-TEST
  auto it = base::ranges::find(app_infos, outer_app_id,
                               &apps::IntentPickerAppInfo::launch_name);
  ASSERT_NE(it, app_infos.end());
  size_t index = it - app_infos.begin();

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  views::test::ButtonTestApi test_api(
      web_app::GetIntentPickerButtonAtIndex(index));
  test_api.NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  web_app::intent_picker_bubble()->AcceptDialog();

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IntentPickerViewAcceptLaunchApp"));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsWebApp(app_browser));
  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(app_browser, outer_app_id));
}

IN_PROC_BROWSER_TEST_F(DefaultLinkCapturingInteractiveUiTest,
                       IntentPickerBubbleCancel) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));
  web_app::intent_picker_bubble()->CancelDialog();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IntentPickerViewClosedStayInChrome"));
  // Verify no new browsers have opened.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(DefaultLinkCapturingInteractiveUiTest,
                       IntentPickerBubbleIgnored) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));
  // Opening a new tab should ignore the current intent picker view.
  chrome::NewTab(browser());

  EXPECT_EQ(1, user_action_tester.GetActionCount("IntentPickerViewIgnored"));
  // Verify no new browsers have opened.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

#if BUILDFLAG(IS_MAC)
using IntentPickerInteractiveUiTest = DefaultLinkCapturingInteractiveUiTest;
// Test that if there is a Universal Link for a site, it shows the picker
// with the app icon.
IN_PROC_BROWSER_TEST_F(IntentPickerInteractiveUiTest,
                       ShowUniversalLinkAppInIntentChip) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  const char* kFinderAppPath = "/System/Library/CoreServices/Finder.app";

  // Start with a page with no corresponding native app.
  apps::OverrideMacAppForUrlForTesting(true, "");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Verify that no icon was shown.
  EXPECT_TRUE(web_app::AwaitIntentPickerTabHelperIconUpdateComplete(
      browser()->tab_strip_model()->GetActiveWebContents()));
  ASSERT_FALSE(web_app::GetIntentPickerIcon(browser())->GetVisible());

  // Load a different page while simulating it having a native app.
  apps::OverrideMacAppForUrlForTesting(true, kFinderAppPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Verify app icon shows up in the intent picker.
  EXPECT_TRUE(web_app::AwaitIntentPickerTabHelperIconUpdateComplete(
      browser()->tab_strip_model()->GetActiveWebContents()));
  IntentChipButton* intent_picker_icon =
      web_app::GetIntentPickerIcon(browser());
  ASSERT_NE(intent_picker_icon, nullptr);

  ui::ImageModel app_icon = intent_picker_icon->GetAppIconForTesting();

  SkColor final_color = app_icon.GetImage().AsBitmap().getColor(8, 8);
  EXPECT_TRUE(
      web_app::AreColorsEqual(SK_ColorRED, final_color, /*threshold=*/50));
}
#endif  // BUILDFLAG(IS_MAC)

// TODO(b/338969664): The following tests are failing on Linux MSan/macOS 14, so
// disabling for now. These should be re-enabled.
#if (BUILDFLAG(IS_LINUX) &&                                       \
     (defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER))) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_AcceptingBubbleMeasuresUserAccept \
  DISABLED_AcceptingBubbleMeasuresUserAccept
#define MAYBE_BubbleDismissMeasuresUserDismiss \
  DISABLED_BubbleDismissMeasuresUserDismiss
#define MAYBE_ClosingAppWindowMeasuresDismiss \
  DISABLED_ClosingAppWindowMeasuresDismiss
#define MAYBE_IPHShownOnLinkClick DISABLED_IPHShownOnLinkClick
#else
#define MAYBE_AcceptingBubbleMeasuresUserAccept \
  AcceptingBubbleMeasuresUserAccept
#define MAYBE_BubbleDismissMeasuresUserDismiss BubbleDismissMeasuresUserDismiss
#define MAYBE_ClosingAppWindowMeasuresDismiss ClosingAppWindowMeasuresDismiss
#define MAYBE_IPHShownOnLinkClick IPHShownOnLinkClick
#endif

// Test to verify that launching an app from the intent picker chip shows the
// IPH bubble if the feature flag is enabled.
class WebAppLinkCapturingIPHPromoTest
    : public InteractiveFeaturePromoTestT<
          DefaultLinkCapturingInteractiveUiTest>,
      public testing::WithParamInterface<bool> {
 public:
  WebAppLinkCapturingIPHPromoTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch})) {}

 protected:
  GURL GetAppUrl() {
    return embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  }

  webapps::AppId InstallApp() {
    GURL start_url = GetAppUrl();
    webapps::AppId app_id =
        web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
    return app_id;
  }

  bool LinkCapturingEnabled() { return GetParam(); }

  void Navigate(Browser* browser,
                const GURL& url,
                LinkTarget link_target = LinkTarget::SELF) {
    ClickLinkAndWait(browser->tab_strip_model()->GetActiveWebContents(), url,
                     link_target, "");
  }

  user_education::HelpBubbleView* GetCurrentPromoBubble(Browser* browser) {
    return views::ElementTrackerViews::GetInstance()
        ->GetFirstMatchingViewAs<user_education::HelpBubbleView>(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
            browser->window()->GetElementContext());
  }

  void SetUpSiteForLinkCapturingIphBubble(const webapps::AppId& app_id) {
    EXPECT_EQ(
        apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id),
        base::ok());
  }

  void AcceptCustomActionIPH(Browser* app_browser) {
    RunTestSequenceInContext(app_browser->window()->GetElementContext(),
                             PressNonDefaultPromoButton());
  }

  void DismissIPH(Browser* app_browser) {
    RunTestSequenceInContext(app_browser->window()->GetElementContext(),
                             PressDefaultPromoButton());
  }

  Browser* TriggerAppLaunchIphAndGetBrowser(const webapps::AppId& app_id) {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    Navigate(browser(), GetAppUrl());

    Browser* app_browser = browser_added_waiter.Wait();
    EXPECT_TRUE(
        web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
    EXPECT_NE(browser(), app_browser);
    RunTestSequenceInContext(
        app_browser->window()->GetElementContext(),
        WaitForPromo(feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch));
    return app_browser;
  }
};

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingIPHPromoTest,
                       AppLaunchFromIntentChipShowsIPH) {
  const webapps::AppId app_id = InstallApp();

  if (LinkCapturingEnabled()) {
    EXPECT_EQ(
        apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id),
        base::ok());
  } else {
    EXPECT_EQ(
        apps::test::DisableLinkCapturingByUser(browser()->profile(), app_id),
        base::ok());
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  if (LinkCapturingEnabled()) {
    EXPECT_TRUE(web_app::WaitForIPHToShowIfAny(app_browser));
  }

  EXPECT_EQ(app_browser->window()->IsFeaturePromoActive(
                feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch),
            LinkCapturingEnabled());
}

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingIPHPromoTest,
                       MAYBE_IPHShownOnLinkClick) {
  const webapps::AppId app_id = InstallApp();
  SetUpSiteForLinkCapturingIphBubble(app_id);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  Navigate(browser(), GetAppUrl());

  Browser* app_browser = browser_added_waiter.Wait();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
  EXPECT_NE(browser(), app_browser);
  EXPECT_TRUE(web_app::WaitForIPHToShowIfAny(app_browser));
  EXPECT_TRUE(app_browser->window()->IsFeaturePromoActive(
      feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch));
}

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingIPHPromoTest,
                       MAYBE_ClosingAppWindowMeasuresDismiss) {
  const webapps::AppId app_id = InstallApp();
  base::UserActionTester user_action_tester;
  SetUpSiteForLinkCapturingIphBubble(app_id);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(app_id);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  chrome::CloseWindow(app_browser);
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingIPHPromoTest,
                       MAYBE_AcceptingBubbleMeasuresUserAccept) {
  const webapps::AppId app_id = InstallApp();
  base::UserActionTester user_action_tester;
  SetUpSiteForLinkCapturingIphBubble(app_id);
  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(app_id);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  AcceptCustomActionIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleAccepted"));
}

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingIPHPromoTest,
                       MAYBE_BubbleDismissMeasuresUserDismiss) {
  const webapps::AppId app_id = InstallApp();
  base::UserActionTester user_action_tester;
  SetUpSiteForLinkCapturingIphBubble(app_id);
  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(app_id);
  DismissIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppLinkCapturingIPHPromoTest,
                         testing::Values(true, false),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "LinkCapturingEnabled"
                                             : "LinkCapturingDisabled";
                         });
