// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"
#endif  // BUILDFLAG(IS_MAC)

// This test only runs on Windows, Mac and Linux platforms.
class DefaultIntentPickerBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  DefaultIntentPickerBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(), {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  IntentChipButton* GetIntentPickerIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetIntentChipButton();
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  testing::AssertionResult AwaitIntentPickerTabHelperIconUpdateComplete() {
    base::test::TestFuture<void> future;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
    tab_helper->SetIconUpdateCallbackForTesting(  // IN-TEST
        future.GetCallback(), /*include_latest_navigation=*/true);
    if (!future.Wait()) {
      return testing::AssertionFailure()
             << "Intent picker app did not resolve an applicable app.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult WaitForIntentPickerToShow() {
    auto result = AwaitIntentPickerTabHelperIconUpdateComplete();
    if (!result) {
      return result;
    }
    IntentChipButton* intent_picker_icon = GetIntentPickerIcon();
    if (!intent_picker_icon) {
      return testing::AssertionFailure()
             << "Intent picker icon does not exist.";
    }

    if (!intent_picker_icon->GetVisible()) {
      web_app::IntentChipVisibilityObserver intent_chip_visibility_observer(
          intent_picker_icon);
      intent_chip_visibility_observer.WaitForChipToBeVisible();
      if (!intent_picker_icon->GetVisible()) {
        return testing::AssertionFailure()
               << "Intent picker icon never became visible.";
      }
    }

    return testing::AssertionSuccess();
  }

  testing::AssertionResult ClickIntentPickerChip() {
    auto result = WaitForIntentPickerToShow();
    if (!result) {
      return result;
    }

    views::test::ButtonTestApi test_api(GetIntentPickerIcon());
    test_api.NotifyClick(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    return testing::AssertionSuccess();
  }

  testing::AssertionResult ClickIntentPickerAndWaitForBubble() {
    auto result = WaitForIntentPickerToShow();
    if (!result) {
      return result;
    }

    views::NamedWidgetShownWaiter intent_picker_bubble_shown(
        views::test::AnyWidgetTestPasskey{},
        IntentPickerBubbleView::kViewClassName);
    views::test::ButtonTestApi test_api(GetIntentPickerIcon());
    test_api.NotifyClick(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

    if (!intent_picker_bubble_shown.WaitIfNeededAndGet()) {
      return testing::AssertionFailure()
             << "Intent picker bubble did not appear after click.";
    }

    EXPECT_NE(intent_picker_bubble(), nullptr)
        << "intent picker not initialized";
    return testing::AssertionSuccess();
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

  views::Button* GetIntentPickerButtonAtIndex(size_t index) {
    EXPECT_NE(intent_picker_bubble(), nullptr)
        << " intent picker bubble not initialized";
    auto children =
        intent_picker_bubble()
            ->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
            ->children();
    EXPECT_LE(index, children.size());
    return static_cast<views::Button*>(children[index]);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultIntentPickerBrowserTest,
                       IntentPickerBubbleAcceptCorrectActions) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  EXPECT_TRUE(ClickIntentPickerAndWaitForBubble());

  base::UserActionTester user_action_tester;
  // The app list is currently not deterministically ordered, so find the
  // correct item and select that.
  const auto& app_infos =
      intent_picker_bubble()->app_info_for_testing();  // IN-TEST
  auto it = base::ranges::find(app_infos, outer_app_id,
                               &apps::IntentPickerAppInfo::launch_name);
  ASSERT_NE(it, app_infos.end());
  size_t index = it - app_infos.begin();

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  views::test::ButtonTestApi test_api(GetIntentPickerButtonAtIndex(index));
  test_api.NotifyClick(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  intent_picker_bubble()->AcceptDialog();

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IntentPickerViewAcceptLaunchApp"));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsWebApp(app_browser));
  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(app_browser, outer_app_id));
}

IN_PROC_BROWSER_TEST_F(DefaultIntentPickerBrowserTest,
                       IntentPickerBubbleCancel) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(ClickIntentPickerAndWaitForBubble());
  intent_picker_bubble()->CancelDialog();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IntentPickerViewClosedStayInChrome"));
  // Verify no new browsers have opened.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(DefaultIntentPickerBrowserTest,
                       IntentPickerBubbleIgnored) {
  const auto [outer_app_id, inner_app_id] = InstallOuterAppAndInnerApp();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetNestedPageUrl()));
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(ClickIntentPickerAndWaitForBubble());
  // Opening a new tab should ignore the current intent picker view.
  chrome::NewTab(browser());

  EXPECT_EQ(1, user_action_tester.GetActionCount("IntentPickerViewIgnored"));
  // Verify no new browsers have opened.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

#if BUILDFLAG(IS_MAC)
using IntentPickerBrowserTestMac = DefaultIntentPickerBrowserTest;
// Test that if there is a Universal Link for a site, it shows the picker icon
// and lists the app as an option in the bubble.
IN_PROC_BROWSER_TEST_F(IntentPickerBrowserTestMac,
                       ShowUniversalLinkInIntentPicker) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  const char* kFinderAppName = "Finder";
  const char* kFinderAppPath = "/System/Library/CoreServices/Finder.app";

  // Start with a page with no corresponding native app.
  apps::OverrideMacAppForUrlForTesting(true, "");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Verify that no icon was shown.
  EXPECT_TRUE(AwaitIntentPickerTabHelperIconUpdateComplete());
  ASSERT_FALSE(GetIntentPickerIcon()->GetVisible());

  // Load a different page while simulating it having a native app.
  apps::OverrideMacAppForUrlForTesting(true, kFinderAppPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Verify intent picker chip shows up, click on it, and wait for the bubble to
  // be populated
  EXPECT_TRUE(ClickIntentPickerAndWaitForBubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();  // IN-TEST
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(kFinderAppPath, app_info[0].launch_name);
  EXPECT_EQ(kFinderAppName, app_info[0].display_name);

  // Navigate to the first page while simulating no native app.
  apps::OverrideMacAppForUrlForTesting(true, "");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Verify that the icon was hidden.
  ASSERT_FALSE(GetIntentPickerIcon()->GetVisible());
}
#endif  // BUILDFLAG(IS_MAC)
