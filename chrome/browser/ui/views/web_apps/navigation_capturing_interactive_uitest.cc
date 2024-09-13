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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
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
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace {
constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kToSiteATargetBlankWithOpener[] = "id-LINK-A_TO_A-BLANK-OPENER";
constexpr char kToSiteBTargetBlankNoopener[] = "id-LINK-A_TO_B-BLANK-NO_OPENER";
constexpr char kToSiteBTargetBlankWithOpener[] = "id-LINK-A_TO_B-BLANK-OPENER";
}  // namespace

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

 protected:
  // The method of interacting with the element:
  enum class ClickMethod { kLeftClick, kMiddleClick, kShiftClick };

  // This function simulates a click on the middle of an element matching
  // `element_id` based on the type of click passed to it.
  void SimulateClickOnElement(content::WebContents* contents,
                              std::string element_id,
                              ClickMethod click) {
    gfx::Point element_center = gfx::ToFlooredPoint(
        content::GetCenterCoordinatesOfElementWithId(contents, element_id));
    int modifiers = 0;
    blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
    switch (click) {
      case ClickMethod::kLeftClick:
        modifiers = blink::WebInputEvent::Modifiers::kNoModifiers;
        break;
      case ClickMethod::kMiddleClick:
#if BUILDFLAG(IS_MAC)
        modifiers = blink::WebInputEvent::Modifiers::kMetaKey;
#else
        modifiers = blink::WebInputEvent::Modifiers::kControlKey;
#endif  // BUILDFLAG(IS_MAC)
        break;
      case ClickMethod::kShiftClick:
        modifiers = blink::WebInputEvent::Modifiers::kShiftKey;
        break;
    }
    content::SimulateMouseClickAt(contents, modifiers, button, element_center);
  }

  GURL GetStartUrl() {
    return embedded_test_server()->GetURL(kStartPageScopeA);
  }

  GURL GetDestinationUrl() {
    return embedded_test_server()->GetURL(kDestinationPageScopeB);
  }

  webapps::AppId InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->launch_handler = blink::Manifest::LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kAuto);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    const webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(browser()->profile(), app_id).Await();
    return app_id;
  }

  BrowserFeaturePromoController* GetFeaturePromoController(Browser* browser) {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser->window()->GetFeaturePromoController());
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
                                            ClickMethod click,
                                            const std::string& elementId) {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    SimulateClickOnElement(contents, elementId, click);

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
  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration override_registration_;
};

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       IPHShownOnLinkLeftClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       IPHShownOnLinkMiddleClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetStartUrl());

  content::WebContents* contents = OpenStartPageInApp(app_id);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kMiddleClick, kToSiteATargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       IPHShownOnLinkShiftClick) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());
  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kShiftClick, kToSiteBTargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id_b));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       IPHShownOnAuxContext) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());

  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kLeftClick, kToSiteBTargetBlankWithOpener);
  ASSERT_NE(nullptr, app_browser);

  EXPECT_FALSE(IsNavCapturingIphVisible(/*expect_visible=*/false, app_browser,
                                        app_id_b));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       ClosingAppWindowMeasuresDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  chrome::CloseWindow(app_browser);
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       AcceptingBubbleMeasuresUserAccept) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIPHAppBubbleShown"));

  AcceptCustomActionIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIPHPromoTest,
                       BubbleDismissMeasuresUserDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);

  Browser* app_browser = TriggerAppLaunchIphAndGetBrowser(
      contents, ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  EXPECT_TRUE(
      IsNavCapturingIphVisible(/*expect_visible=*/true, app_browser, app_id));
  DismissIPH(app_browser);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingIPHAppBubbleNotAccepted"));
}
