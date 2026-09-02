// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/omnibox/browser/open_tab_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

namespace {

void RequestPermission(BrowserWindowInterface* browser) {
  test::PermissionRequestManagerTestApi test_api(browser);
  permissions::PermissionRequestObserver observer(
      browser->GetTabStripModel()->GetActiveWebContents());

  EXPECT_NE(nullptr, test_api.manager());
  test_api.AddSimpleRequest(browser->GetTabStripModel()
                                ->GetActiveWebContents()
                                ->GetPrimaryMainFrame(),
                            permissions::RequestType::kGeolocation);

  observer.Wait();
}

LocationBar* GetLocationBar(BrowserWindowInterface* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)->GetLocationBar();
}

}  // namespace

class PermissionRequestChipGestureSensitiveBrowserTest
    : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ChipFinalizedWhenInteractingWithOmnibox) {
  RequestPermission(browser());
  LocationBar* lb = GetLocationBar(browser());
  auto* animation =
      views::AsViewClass<PermissionChipView>(
          views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
              PermissionChipView::kPermissionRequestChipElementId,
              views::ElementTrackerViews::GetContextForView(
                  BrowserView::GetBrowserViewForBrowser(browser()))))
          ->animation_for_testing();

  // Animate the chip expand.
  gfx::AnimationTestApi animation_api(animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + animation->GetSlideDuration());

  // After animation ended, the chip is expanded and the bubble is shown because
  // the gesture sensitive request feature is enabled.
  EXPECT_TRUE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_TRUE(lb->GetChipController()->IsBubbleShowing());

  // Because the bubble is shown, callback timers should be abandoned
  EXPECT_FALSE(
      lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()->is_dismiss_timer_running_for_testing());

  // Type something in the omnibox.
  auto* omnibox_view = lb->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  lb->GetOmniboxController()->edit_model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // While the user is interacting with the omnibox, the chip is hidden, the
  // location icon isn't offset by the chip and the bubble is hidden.
  EXPECT_FALSE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(lb->GetChipController()->IsBubbleShowing());

  // Ensure no callbacks are pending.
  EXPECT_FALSE(
      lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()->is_dismiss_timer_running_for_testing());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ChipIsNotShownWhenInteractingWithOmnibox) {
  LocationBar* lb = GetLocationBar(browser());

  // The chip is not shown because there is no active permission request.
  EXPECT_FALSE(lb->GetChipController()->IsPermissionPromptChipVisible());

  // Type something in the omnibox.
  auto* omnibox_view = lb->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  lb->GetOmniboxController()->edit_model()->SetInputInProgress(true);

  RequestPermission(browser());

  // While the user is interacting with the omnibox, an incoming permission
  // request will be automatically ignored. The chip is not shown.
  EXPECT_FALSE(lb->GetChipController()->IsPermissionPromptChipVisible());
}

// This is an end-to-end test that verifies that a permission prompt bubble will
// not be shown because of the empty address bar. Under the normal conditions
// such a test should be placed in PermissionsSecurityModelInteractiveUITest but
// due to dependency issues (see crbug.com/40709781) `//chrome/browser` is not
// allowed to have dependencies on `//chrome/browser/ui/views/*`.
IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       PermissionRequestIsAutoIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;

  content::WebContents* embedder_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);

  constexpr char kCheckMicrophone[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'microphone'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

  constexpr char kRequestMicrophone[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

  EXPECT_EQ(false, content::EvalJs(main_rfh, kCheckMicrophone,
                                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));

  LocationBar* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();

  // Type something in the omnibox.
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  location_bar->GetOmniboxController()->edit_model()->SetInputInProgress(true);

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  permissions::PermissionRequestObserver observer(embedder_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_TRUE(content::ExecJs(
      main_rfh, kRequestMicrophone,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until a permission request is shown or finalized.
  observer.Wait();

  // Permission request was is in progress without showing a prompt bubble.
  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_FALSE(observer.request_shown());
  EXPECT_TRUE(observer.is_view_recreate_failed());
  EXPECT_FALSE(manager->GetCurrentPrompt());

  EXPECT_EQ(false, content::EvalJs(main_rfh, kCheckMicrophone,
                                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.AudioCapture.Gesture.Attempt", false, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ShouldUpdateActiverPRMAndObservations) {
  constexpr char kRequestNotifications[] = R"(
    new Promise(resolve => {
      Notification.requestPermission().then(function (permission) {
        resolve(permission)
      });
    })
    )";

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  // Setup: open 2 tabs at the same origin
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents::FromRenderFrameHost(rfh_tab_0)->Focus();
  embedder_contents_tab_0 = tab_strip->GetActiveWebContents();
  auto* manager_tab_0 = permissions::PermissionRequestManager::FromWebContents(
      embedder_contents_tab_0);
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);

  chrome::NewTabToRight(browser());
  EXPECT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(1);
  content::RenderFrameHost* rfh_tab_1 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents* embedder_contents_tab_1 =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto* manager_tab_1 = permissions::PermissionRequestManager::FromWebContents(
      embedder_contents_tab_1);
  permissions::PermissionRequestObserver observer_tab_1(
      embedder_contents_tab_1);

  tab_strip->ActivateTabAt(0);

  // Obtain the chip controller instance. The chip controller instance is tied
  // to the location bar view instance. Since the location bar view instance is
  // reused across multiple tabs, this in turn also means that the chip
  // controller instance is the same across multiple tabs.
  LocationBar* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
  ASSERT_TRUE(location_bar);
  ChipController* chip_controller = location_bar->GetChipController();

  // Trigger permission request on first tab
  EXPECT_FALSE(manager_tab_0->IsRequestInProgress());
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();
  EXPECT_TRUE(manager_tab_0->IsRequestInProgress());

  // Close first tab
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_FALSE(manager_tab_1->IsRequestInProgress());

  // After closing the first tab, the chip controller should no longer be
  // observing any permission request manager. It should also no longer hold a
  // reference to a Permission Request Manager instance.
  ASSERT_FALSE(chip_controller->permissions::PermissionRequestManager::
                   Observer::IsInObserverList());
  ASSERT_FALSE(
      chip_controller->active_permission_request_manager().has_value());

  // Trigger a request on the second (the now only remaining) tab.
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_1, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  observer_tab_1.Wait();

  // During the request, the chip controller should be observing the correct
  // permission request manager instance, and have a reference to the same.
  EXPECT_TRUE(manager_tab_1->IsRequestInProgress());
  ASSERT_TRUE(chip_controller->active_permission_request_manager().has_value());
  ASSERT_EQ(chip_controller->active_permission_request_manager().value(),
            manager_tab_1);
  ASSERT_TRUE(manager_tab_1->get_observer_list_for_testing()->HasObserver(
      chip_controller));
}

class PermissionRequestChipGestureInsensitiveBrowserTest
    : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureInsensitiveBrowserTest,
                       CallbacksResetWhenInteractingWithOmnibox) {
  RequestPermission(browser());
  LocationBar* lb = GetLocationBar(browser());
  auto* animation =
      views::AsViewClass<PermissionChipView>(
          views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
              PermissionChipView::kPermissionRequestChipElementId,
              views::ElementTrackerViews::GetContextForView(
                  BrowserView::GetBrowserViewForBrowser(browser()))))
          ->animation_for_testing();

  // Animate the chip expand.
  gfx::AnimationTestApi animation_api(animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + animation->GetSlideDuration());

  // After animation ended, the chip is expanded and a bubble is shown.
  EXPECT_TRUE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_TRUE(lb->GetChipController()->IsBubbleShowing());

  // Because a bubble is shown, the collapse callback timer should not be
  // running.
  EXPECT_FALSE(
      lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()->is_dismiss_timer_running_for_testing());

  // Type something in the omnibox.
  auto* omnibox_view = lb->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  lb->GetOmniboxController()->edit_model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // Ensure chip is no longer visible and callbacks are no longer running.
  EXPECT_FALSE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(
      lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()->is_dismiss_timer_running_for_testing());
}

class PermissionRequestChipBrowserUiTest : public UiBrowserTest {
 public:
  PermissionRequestChipBrowserUiTest() = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    RequestPermission(browser());
  }

  bool VerifyUi() override {
    LocationBar* const location_bar =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
    PermissionChipInterface* const chip =
        location_bar->GetChipController()->chip();
    if (!chip->GetVisible() || chip->IsFullyCollapsed()) {
      return false;
    }

    auto* const element =
        BrowserElements::From(browser())->GetElement(kLocationBarElementId);
    EXPECT_NE(element, nullptr);
    if (!element) {
      return false;
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(element, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::BrowserDestroyedObserver().Wait();
  }

 private:
  // Disable the permission chip animation. This happens automatically in pixel
  // test mode, but without doing this explicitly, the test will fail when run
  // interactively.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

// Flaky b/40261456
IN_PROC_BROWSER_TEST_F(PermissionRequestChipBrowserUiTest,
                       DISABLED_InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// This test verifies that the confirmation chip is hidden after it collapses
// even if animation is disabled.
IN_PROC_BROWSER_TEST_F(PermissionRequestChipBrowserUiTest,
                       TestDisabledAnimation) {
  RequestPermission(browser());
  LocationBar* lb = GetLocationBar(browser());

  // The chip is expanded and a bubble is shown.
  EXPECT_TRUE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_TRUE(lb->GetChipController()->IsBubbleShowing());

  lb->GetChipController()->active_permission_request_manager().value()->Deny(
      /*prompt_options=*/std::monostate());

  base::RunLoop().RunUntilIdle();

  // The chip is visible as we show the confirmation.
  EXPECT_TRUE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(lb->GetChipController()->IsBubbleShowing());
  EXPECT_TRUE(lb->GetChipController()->is_confirmation_showing());
  EXPECT_TRUE(lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()
                   ->is_waiting_for_confirmation_collapse_for_testing());

  lb->GetChipController()->fire_collapse_timer_for_testing();

  EXPECT_FALSE(lb->GetChipController()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(lb->GetChipController()->is_confirmation_showing());
  EXPECT_FALSE(
      lb->GetChipController()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lb->GetChipController()
                   ->is_waiting_for_confirmation_collapse_for_testing());
}

class PermissionRequestChipSensorBrowserTest
    : public InProcessBrowserTest,
      public content::TestDevToolsProtocolClient {
 public:
  PermissionRequestChipSensorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kSensorsAllowAskBlockPermissionModel,
         content_settings::features::kLeftHandSideSensorActivityIndicators},
        {});
  }

  ~PermissionRequestChipSensorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ssl_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.AddDefaultHandlers(GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(ssl_server_.Start());
  }

 protected:
  net::EmbeddedTestServer ssl_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipSensorBrowserTest,
                       SensorsIndicatorCDP) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  AttachToWebContents(web_contents);

  // Navigate to secure page.
  GURL url = ssl_server_.GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(true, content::EvalJs(web_contents, "isSecureContext"));

  // Grant permission first.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ALLOW);

  // Enable virtual sensor override via CDP.
  base::DictValue params;
  params.Set("type", "gyroscope");
  params.Set("enabled", true);
  SendCommandSync("Emulation.setSensorOverrideEnabled", std::move(params));

  // Execute JS to start sensor. Store on window so we can stop it later.
  constexpr char kStartSensor[] = R"(
    new Promise(resolve => {
      window.sensor = new Gyroscope({frequency: 10});
      window.sensor.addEventListener('activate', () => resolve('active'));
      window.sensor.addEventListener(
          'error', (e) => resolve('error: ' + e.error.message));
      window.sensor.start();
    })
  )";

  EXPECT_EQ("active", content::EvalJs(web_contents, kStartSensor));

  PermissionDashboardController* dashboard_controller =
      GetLocationBar(browser())->GetPermissionDashboardController();
  ASSERT_TRUE(dashboard_controller);

  PermissionChipInterface* indicator_chip =
      dashboard_controller->permission_dashboard()->GetIndicatorChip();

  // The indicator chip should be visible.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return indicator_chip->GetVisible(); }));

  EXPECT_EQ(indicator_chip->GetTextForTesting(),
            l10n_util::GetStringUTF16(IDS_SENSORS_IN_USE));
  EXPECT_EQ(indicator_chip->GetThemeForTesting(),
            PermissionChipTheme::kInUseActivityIndicator);

  // Navigate away to destroy the document and close Mojo pipes.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Wait until the indicator chip is hidden.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !indicator_chip->GetVisible(); }));

  DetachProtocolClient();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipSensorBrowserTest,
                       SensorsIndicatorSuppressedByMediaCDP) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  AttachToWebContents(web_contents);

  // Navigate to secure page.
  GURL url = ssl_server_.GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Grant sensor permission.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ALLOW);

  content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents->GetPrimaryMainFrame())
      ->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  // Enable virtual sensor override via CDP and start Gyroscope.
  base::DictValue params;
  params.Set("type", "gyroscope");
  params.Set("enabled", true);
  SendCommandSync("Emulation.setSensorOverrideEnabled", std::move(params));

  constexpr char kStartSensor[] = R"(
    new Promise(resolve => {
      window.sensor = new Gyroscope({frequency: 10});
      window.sensor.addEventListener('activate', () => resolve('active'));
      window.sensor.start();
    })
  )";
  EXPECT_EQ("active", content::EvalJs(web_contents, kStartSensor));

  PermissionDashboardController* dashboard_controller =
      GetLocationBar(browser())->GetPermissionDashboardController();
  ASSERT_TRUE(dashboard_controller);

  PermissionChipInterface* indicator_chip =
      dashboard_controller->permission_dashboard()->GetIndicatorChip();

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return indicator_chip->GetVisible(); }));

  // Verify that the active chip text displays Camera usage.
  EXPECT_EQ(indicator_chip->GetTextForTesting(),
            l10n_util::GetStringUTF16(IDS_CAMERA_IN_USE));

  // Reset capturing state and navigate away to clean up.
  content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents->GetPrimaryMainFrame())
      ->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  DetachProtocolClient();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipSensorBrowserTest,
                       SensorsBlockedIndicatorCDP) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  AttachToWebContents(web_contents);

  // Navigate to secure page.
  GURL url = ssl_server_.GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(true, content::EvalJs(web_contents, "isSecureContext"));

  // Block permission.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_BLOCK);

  // Enable virtual sensor override via CDP.
  base::DictValue params;
  params.Set("type", "gyroscope");
  params.Set("enabled", true);
  SendCommandSync("Emulation.setSensorOverrideEnabled", std::move(params));

  // Execute JS to start sensor. We expect it to fail.
  constexpr char kStartSensor[] = R"(
    new Promise(resolve => {
      window.sensor = new Gyroscope({frequency: 10});
      window.sensor.addEventListener('activate', () => resolve('active'));
      window.sensor.addEventListener(
          'error', (e) => resolve('error: ' + e.error.name));
      window.sensor.start();
    })
  )";

  // The error name should be NotAllowedError.
  EXPECT_EQ("error: NotAllowedError",
            content::EvalJs(web_contents, kStartSensor));

  PermissionDashboardController* dashboard_controller =
      GetLocationBar(browser())->GetPermissionDashboardController();
  ASSERT_TRUE(dashboard_controller);

  PermissionChipInterface* indicator_chip =
      dashboard_controller->permission_dashboard()->GetIndicatorChip();

  // The indicator chip should be visible.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return indicator_chip->GetVisible(); }));

  EXPECT_EQ(indicator_chip->GetTextForTesting(),
            l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED));
  EXPECT_EQ(indicator_chip->GetThemeForTesting(),
            PermissionChipTheme::kBlockedActivityIndicator);

  // Navigate away to destroy the document and close Mojo pipes.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Wait until the indicator chip is hidden.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !indicator_chip->GetVisible(); }));

  DetachProtocolClient();
}
