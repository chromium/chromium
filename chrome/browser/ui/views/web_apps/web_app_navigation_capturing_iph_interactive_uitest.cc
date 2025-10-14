// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/debug_info_printer.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/interaction/dom_message_observer.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace web_app {

using apps::test::LinkCapturingFeatureVersion;

namespace {
constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kToSiteATargetBlankWithOpener[] = "id-LINK-A_TO_A-BLANK-OPENER";
constexpr char kToSiteBTargetBlankNoOpener[] = "id-LINK-A_TO_B-BLANK-NO_OPENER";
constexpr char kToSiteBTargetBlankWithOpener[] = "id-LINK-A_TO_B-BLANK-OPENER";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kStartPageId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewPageId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAppPageId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDestinationPageId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(LatestDomMessageObserver,
                                    kLatestDomMessage);

// IPH tests that need the navigation capturing flag to be turned on can use
// this base class to test the IPH functionality. Note: If the intent is to also
// test the IPH with the navigation capturing flag *off*, then use the derived
// class instead (see WebAppNavigationCapturingIphUiTestParameterized below).
class WebAppNavigationCapturingIphUiTest : public InteractiveFeaturePromoTest {
 public:
  WebAppNavigationCapturingIphUiTest()
      : WebAppNavigationCapturingIphUiTest(
            LinkCapturingFeatureVersion::kV2DefaultOn) {}

  explicit WebAppNavigationCapturingIphUiTest(LinkCapturingFeatureVersion flag)
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
             feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab})) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(flag),
        /*disabled_features=*/{});
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

  virtual bool NavigationCapturingV2Enabled() {
    // Base class assumes Navigation Capture flag is always on. For testing with
    // that flag off also, see WebAppNavigationCapturingIphUiTestParameterized
    // below.
    return true;
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

  // Opens app `app_id` in a new window.
  // The context of the last step is the new app window.
  auto OpenApp(const webapps::AppId& app_id) {
    auto steps =
        Steps(InstrumentNextTab(kAppPageId, AnyBrowser()), Do([this, app_id]() {
                web_app::WebAppProvider* provider =
                    web_app::WebAppProvider::GetForLocalAppsUnchecked(
                        browser()->profile());
                provider->scheduler().LaunchAppWithCustomParams(
                    apps::AppLaunchParams(
                        app_id, apps::LaunchContainer::kLaunchContainerWindow,
                        WindowOpenDisposition::CURRENT_TAB,
                        apps::LaunchSource::kFromTest),
                    base::DoNothing());
              }),
              InAnyContext(WaitForShow(kAppPageId)));
    AddDescriptionPrefix(steps, base::StrCat({"OpenApp( ", app_id, " )"}));
    return steps;
  }

  auto WaitForLaunchQueuesFlushedAndNavigationComplete() {
    auto steps = Steps(
        WaitForState(kLatestDomMessage,
                     testing::HasSubstr("PleaseFlushLaunchQueue")),
        Do([]() { apps::test::FlushLaunchQueuesForAllBrowserTabs(); }),
        CheckResult(&apps::test::ResolveWebContentsWaitingForLaunchQueueFlush,
                    base::test::HasValue(),
                    "Javascript error/s while notifying pages that the launch "
                    "queue was flushed."),
        WaitForState(kLatestDomMessage,
                     testing::HasSubstr("FinishedNavigating")));
    AddDescriptionPrefix(steps,
                         "WaitForLaunchQueuesFlushedAndNavigationComplete()");
    return steps;
  }

  // Opens the "start" page for app testing, with links to launch various apps.
  auto OpenStartPage() {
    auto steps = Steps(InstrumentTab(kStartPageId),
                       ObserveState(kLatestDomMessage, kStartPageId),
                       NavigateWebContents(kStartPageId, GetStartUrl()),
                       WaitForLaunchQueuesFlushedAndNavigationComplete());
    AddDescriptionPrefix(steps, "OpenStartPage()");
    return steps;
  }

  // Opens the "start" page for app testing, with links to launch various apps,
  // in its own app with `app_id`. The context of the last step is the new app
  // window.
  auto OpenAppStartPage(const webapps::AppId& app_id) {
    auto steps = Steps(
        InstrumentNextTab(kStartPageId, AnyBrowser()), Do([this, app_id]() {
          WebAppProvider* provider =
              WebAppProvider::GetForWebApps(browser()->profile());
          CHECK(provider);
          provider->scheduler().LaunchApp(app_id, /*url=*/std::nullopt,
                                          base::DoNothing());
        }),
        InAnyContext(WaitForShow(kStartPageId)),
        InSameContext(ObserveState(kLatestDomMessage, kStartPageId),
                      WaitForLaunchQueuesFlushedAndNavigationComplete()));
    AddDescriptionPrefix(steps, "OpenAppStartPage()");
    return steps;
  }

  // Clicks on the "launch app" link on the start page with element ID
  // `element_id`. The start page must be open in at least one browser. The
  // context of the last step is the browser window containing the start page.
  auto ClickLaunchLink(
      const std::string& element_id,
      ui_controls::MouseButton button,
      ui_controls::AcceleratorState accel = ui_controls::kNoAccelerator) {
    return InAnyContext(
        ClickElement(kStartPageId, {"#" + element_id}, button, accel)
            .SetDescription("ClickLaunchLink()"));
  }

  auto TriggerNavigateExisting(
      const std::string& element_id,
      ui_controls::MouseButton button,
      ui_controls::AcceleratorState accel = ui_controls::kNoAccelerator) {
    auto steps = Steps(ClickLaunchLink(element_id, button, accel),
                       InAnyContext(WaitForShow(kDestinationPageId)));
    AddDescriptionPrefix(steps, "TriggerNavigateExisting()");
    return steps;
  }

  // Clicks on `element_id` in the start page, which must be open in at least
  // one browser, launching a new app window. The context of the last step is
  // the window in which the link was opened.
  auto TriggerAppLaunch(
      const std::string& element_id,
      ui_controls::MouseButton button,
      ui_controls::AcceleratorState accel = ui_controls::kNoAccelerator,
      bool expect_new_browser = false) {
    // Note: on Mac, the web contents for a new app can become "visible" well
    // before the browser itself does, which can cause a race condition.
    // Therefore, throughout, we wait for the web contents and not the browser
    // to enforce consistency.
    auto steps =
        Steps(InstrumentNextTab(kDestinationPageId, AnyBrowser()),
              ClickLaunchLink(element_id, button, accel),
              InAnyContext(WaitForShow(kDestinationPageId)),
              InSameContext(CheckViewProperty(
                  kBrowserViewElementId, &BrowserView::browser,
                  testing::Ne(expect_new_browser ? browser() : nullptr))));
    AddDescriptionPrefix(steps, "TriggerAppLaunch()");
    return steps;
  }

  // Checks that the user action with `name` has been emitted `count` times.
  auto CheckActionCount(const std::string& name, int count) {
    return CheckResult(
        [this, name]() { return user_action_tester_.GetActionCount(name); },
        count, base::StringPrintf("CheckActionCount(%s)", name.c_str()));
  }

 private:
  base::TimeTicks start_time_ = base::TimeTicks::Now();
  base::UserActionTester user_action_tester_;

  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration override_registration_;
};

// This class is for testing IPH functionality with the Navigation Capturing
// flag either on/off.
class WebAppNavigationCapturingIphUiTestParameterized
    : public WebAppNavigationCapturingIphUiTest,
      public testing::WithParamInterface<LinkCapturingFeatureVersion> {
 public:
  WebAppNavigationCapturingIphUiTestParameterized()
      : WebAppNavigationCapturingIphUiTest(GetParam()) {}

  bool NavigationCapturingV2Enabled() override {
    return GetParam() == LinkCapturingFeatureVersion::kV2DefaultOn;
  }
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       IPHShownOnLinkLeftClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT),
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContextAs(
             kDestinationPageId,
             WaitForPromo(
                 feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch)))),
      InSameContextAs(
          kDestinationPageId,
          CheckPromoRequested(
              feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
              NavigationCapturingV2Enabled())));
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       IPHShownOnLinkMiddleClick) {
  const webapps::AppId app_id = InstallTestWebApp(GetStartUrl());
  RunTestSequence(
      OpenAppStartPage(app_id),
      TriggerAppLaunch(kToSiteATargetBlankWithOpener,
#if BUILDFLAG(IS_MAC)
                       // Middle click does not work (consistently?)
                       // on Mac; see http://crbug.com/366580804
                       ui_controls::LEFT, ui_controls::kCommand
#else
                       ui_controls::MIDDLE
#endif
                       ),
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContextAs(
             kDestinationPageId,
             WaitForPromo(
                 feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch)))),
      InSameContextAs(
          kDestinationPageId,
          CheckPromoRequested(
              feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
              NavigationCapturingV2Enabled())));
}

// TODO(crbug.com/433312075): Shift-click click does not work (consistently?) on
// Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IPHShownOnLinkShiftClick DISABLED_IPHShownOnLinkShiftClick
#else
#define MAYBE_IPHShownOnLinkShiftClick IPHShownOnLinkShiftClick
#endif
IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       MAYBE_IPHShownOnLinkShiftClick) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());
  RunTestSequence(
      OpenAppStartPage(app_id_a),
      TriggerAppLaunch(kToSiteBTargetBlankWithOpener, ui_controls::LEFT,
#if BUILDFLAG(IS_MAC)
                       // Shift-click click does not work (consistently?) on
                       // Mac; see http://crbug.com/366580804
                       static_cast<ui_controls::AcceleratorState>(
                           ui_controls::kCommand | ui_controls::kAlt)
#else
                       ui_controls::kShift
#endif
                           ),
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContextAs(
             kDestinationPageId,
             WaitForPromo(
                 feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch)))),
      InSameContextAs(
          kDestinationPageId,
          CheckPromoRequested(
              feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
              NavigationCapturingV2Enabled())));
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       IPHShownForFocusExisting) {
  const webapps::AppId app_id = InstallTestWebApp(
      GetDestinationUrl(),
      blink::Manifest::LaunchHandler(
          blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting));

  RunTestSequence(
      OpenStartPage(), OpenApp(app_id),
      ClickLaunchLink(kToSiteBTargetBlankNoOpener, ui_controls::LEFT),
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContextAs(
             kAppPageId,
             WaitForPromo(
                 feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch)))),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
          NavigationCapturingV2Enabled())));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIphUiTest,
                       IPHNotShownOnAuxContext) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());

  RunTestSequence(
      OpenAppStartPage(app_id_a),
      TriggerAppLaunch(kToSiteBTargetBlankWithOpener, ui_controls::LEFT),
      InSameContext(CheckPromoRequested(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch, false)));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIphUiTest,
                       ClosingAppWindowMeasuresDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());

  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT),
      InSameContext(
          WaitForPromo(feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch),
          CheckActionCount("LinkCapturingIPHAppBubbleShown", 1),
          WithView(kBrowserViewElementId,
                   [](BrowserView* browser_view) { browser_view->Close(); }),
          WaitForHide(kBrowserViewElementId)),
      CheckActionCount("LinkCapturingIPHAppBubbleNotAccepted", 1));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIphUiTest,
                       AcceptingBubbleMeasuresUserAccept) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT),
      InSameContext(
          WaitForPromo(feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch),
          CheckActionCount("LinkCapturingIPHAppBubbleShown", 1),
          PressNonDefaultPromoButton(),
          CheckActionCount("LinkCapturingIPHAppBubbleAccepted", 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIphUiTest,
                       BubbleDismissMeasuresUserDismiss) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());
  base::UserActionTester user_action_tester;

  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT),
      InSameContext(
          WaitForPromo(feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch),
          PressDefaultPromoButton(),
          CheckActionCount("LinkCapturingIPHAppBubbleNotAccepted", 1)));
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       IPHShownForNavigateExistingAppInTab) {
  webapps::AppId app_id = test::InstallWebApp(
      browser()->profile(),
      WebAppInstallInfo::CreateForTesting(
          GetDestinationUrl(), blink::mojom::DisplayMode::kBrowser,
          mojom::UserDisplayMode::kBrowser,
          blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting));
  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT,
                       ui_controls::kNoAccelerator,
                       /* expect_new_browser= */ false),
      // The second launch is required to trigger the kNavigateExisting behavior
      // and show the IPH.
      TriggerNavigateExisting(kToSiteBTargetBlankNoOpener, ui_controls::LEFT,
                              ui_controls::kNoAccelerator),
      // The app will launch in a new tab in the same browser window, so
      // InSameContext can be used throughout.
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContext(WaitForPromo(
             feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab)))),
      InSameContext(CheckPromoRequested(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
          NavigationCapturingV2Enabled())));
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingIphUiTestParameterized,
                       IPHForAppInTabDisappearsOnNewTabOpen) {
  webapps::AppId app_id = test::InstallWebApp(
      browser()->profile(),
      WebAppInstallInfo::CreateForTesting(
          GetDestinationUrl(), blink::mojom::DisplayMode::kBrowser,
          mojom::UserDisplayMode::kBrowser,
          blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting));
  RunTestSequence(
      OpenStartPage(),
      TriggerAppLaunch(kToSiteBTargetBlankNoOpener, ui_controls::LEFT,
                       ui_controls::kNoAccelerator,
                       /* expect_new_browser= */ false),
      TriggerNavigateExisting(kToSiteBTargetBlankNoOpener, ui_controls::LEFT,
                              ui_controls::kNoAccelerator),
      If([this]() { return NavigationCapturingV2Enabled(); },
         Then(InSameContext(WaitForPromo(
             feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab)))),
      InSameContext(CheckPromoRequested(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
          NavigationCapturingV2Enabled())),
      AddInstrumentedTab(kNewPageId, GURL("https://www.example.com")),
      WaitForWebContentsReady(kNewPageId),
      InSameContextAs(
          kDestinationPageId,
          CheckPromoRequested(
              feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
              false)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppNavigationCapturingIphUiTestParameterized,
    testing::Values(LinkCapturingFeatureVersion::kV2DefaultOn,
                    LinkCapturingFeatureVersion::kV2DefaultOff),
    [](const testing::TestParamInfo<LinkCapturingFeatureVersion>& info) {
      return apps::test::ToString(info.param);
    });

}  // namespace

}  // namespace web_app
