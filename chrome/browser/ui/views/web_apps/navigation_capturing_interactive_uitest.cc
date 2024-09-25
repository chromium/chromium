// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/debug_info_printer.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace web_app {

namespace {
constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kToSiteATargetBlankWithOpener[] = "id-LINK-A_TO_A-BLANK-OPENER";
constexpr char kToSiteBTargetBlankNoopener[] = "id-LINK-A_TO_B-BLANK-NO_OPENER";
constexpr char kToSiteBTargetBlankWithOpener[] = "id-LINK-A_TO_B-BLANK-OPENER";

// Test to verify that the IPH is shown when navigations due to link capture
// occurs.
class WebAppNavigationCapturingIPHPromoTest
    : public InteractiveFeaturePromoTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppNavigationCapturingIPHPromoTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch})) {
    base::FieldTrialParams params;
    params["link_capturing_state"] = "reimpl_default_on";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, params);
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    if (testing::Test::HasFailure()) {
      // Intended to help track down issue 366580804.
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      base::TimeDelta log_time = base::TimeTicks::Now() - start_time_;
      web_app::test::LogDebugInfoToConsole(profile_manager->GetLoadedProfiles(),
                                           log_time);
    }
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

 protected:

  GURL GetStartUrl() {
    return embedded_test_server()->GetURL(kStartPageScopeA);
  }

  GURL GetDestinationUrl() {
    return embedded_test_server()->GetURL(kDestinationPageScopeB);
  }

  webapps::AppId InstallTestWebApp(
      const GURL& start_url,
      blink::Manifest::LaunchHandler input_handler =
          blink::Manifest::LaunchHandler(
              blink::mojom::ManifestLaunchHandler_ClientMode::kAuto)) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->launch_handler = input_handler;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    const webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(browser()->profile(), app_id).Await();
    return app_id;
  }

  BrowserFeaturePromoController* GetFeaturePromoController(Browser* browser) {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser->window()->GetFeaturePromoControllerForTesting());
    return promo_controller;
  }

  user_education::HelpBubbleView* GetCurrentPromoBubble(Browser* browser) {
    auto* const promo_controller = GetFeaturePromoController(browser);
    return promo_controller->promo_bubble_for_testing()
        ->AsA<user_education::HelpBubbleViews>()
        ->bubble_view();
  }

  content::WebContents* OpenStartPageInTab() {
    content::DOMMessageQueue message_queue;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kStartPageScopeA)));

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"ReadyForLinkCaptureTesting\"", message);

    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenStartPageInApp(const webapps::AppId& app_id) {
    content::DOMMessageQueue message_queue;
    auto* const proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    proxy->Launch(app_id,
                  /* event_flags= */ 0, apps::LaunchSource::kFromAppListGrid);
    content::WebContents* contents = waiter.Wait();

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"ReadyForLinkCaptureTesting\"", message);

    return contents;
  }

  void AcceptCustomActionIPH(Browser* app_browser) {
    auto* custom_action_button =
        GetCurrentPromoBubble(app_browser)
            ->GetNonDefaultButtonForTesting(/*index=*/0);
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        custom_action_button, ui::test::InteractionTestUtil::InputType::kMouse);
  }

  void DismissIPH(Browser* app_browser) {
    auto* custom_action_button =
        GetCurrentPromoBubble(app_browser)->GetDefaultButtonForTesting();
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        custom_action_button, ui::test::InteractionTestUtil::InputType::kMouse);
  }

  Browser* TriggerAppLaunchIphAndGetBrowser(content::WebContents* contents,
                                            test::ClickMethod click,
                                            const std::string& elementId) {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    test::SimulateClickOnElement(contents, elementId, click);

    Browser* app_browser = browser_added_waiter.Wait();
    EXPECT_NE(browser(), app_browser);
    return app_browser;
  }

  bool IsNavCapturingIphVisible(bool expect_visible,
                                Browser* app_browser,
                                const webapps::AppId& app_id) {
    if (expect_visible) {
      EXPECT_TRUE(web_app::WaitForIPHToShowIfAny(app_browser));
    }
    return app_browser->window()->IsFeaturePromoActive(
        feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch);
  }

 private:
  base::TimeTicks start_time_ = base::TimeTicks::Now();

  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration override_registration_;
};

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownOnLinkLeftClick DISABLED_IPHShownOnLinkLeftClick
#else
#define MAYBE_IPHShownOnLinkLeftClick IPHShownOnLinkLeftClick
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_IPHShownOnLinkLeftClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownOnLinkMiddleClick DISABLED_IPHShownOnLinkMiddleClick
#else
#define MAYBE_IPHShownOnLinkMiddleClick IPHShownOnLinkMiddleClick
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_IPHShownOnLinkMiddleClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetStartUrl());

  content::WebContents* contents = OpenStartPageInApp(app_id);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kMiddleClick, kToSiteATargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownOnLinkShiftClick DISABLED_IPHShownOnLinkShiftClick
#else
#define MAYBE_IPHShownOnLinkShiftClick IPHShownOnLinkShiftClick
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_IPHShownOnLinkShiftClick) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());
  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kShiftClick, kToSiteBTargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id_b));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownForFocusExisting DISABLED_IPHShownForFocusExisting
#else
#define MAYBE_IPHShownForFocusExisting IPHShownForFocusExisting
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_IPHShownForFocusExisting) {
  const webapps::AppId app_id = InstallTestWebApp(
      GetDestinationUrl(),
      blink::Manifest::LaunchHandler(
          blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting));

  content::WebContents* source_contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, source_contents);

  Browser* browser_b =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_NE(nullptr, browser_b);

  SimulateClickOnElement(source_contents, kToSiteBTargetBlankNoopener,
                         test::ClickMethod::kLeftClick);

  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, browser_b, app_id));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownOnAuxContext DISABLED_IPHShownOnAuxContext
#else
#define MAYBE_IPHShownOnAuxContext IPHShownOnAuxContext
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_IPHShownOnAuxContext) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());

  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);

  EXPECT_FALSE(IsNavCapturingIphVisible(/*expect_visible=*/false, app_browser,
                                        app_id_b));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_ClosingAppWindowMeasuresDismiss \
  DISABLED_ClosingAppWindowMeasuresDismiss
#else
#define MAYBE_ClosingAppWindowMeasuresDismiss ClosingAppWindowMeasuresDismiss
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_ClosingAppWindowMeasuresDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  chrome::CloseWindow(app_browser);
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_AcceptingBubbleMeasuresUserAccept \
  DISABLED_AcceptingBubbleMeasuresUserAccept
#else
#define MAYBE_AcceptingBubbleMeasuresUserAccept \
  AcceptingBubbleMeasuresUserAccept
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_AcceptingBubbleMeasuresUserAccept) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  AcceptCustomActionIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleAccepted"));
}

// Flaky on Mac http://crbug.com/366580804
#if BUILDFLAG(IS_MAC)
#define MAYBE_BubbleDismissMeasuresUserDismiss \
  DISABLED_BubbleDismissMeasuresUserDismiss
#else
#define MAYBE_BubbleDismissMeasuresUserDismiss BubbleDismissMeasuresUserDismiss
#endif
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       MAYBE_BubbleDismissMeasuresUserDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  DismissIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}

}  // namespace

}  // namespace web_app
