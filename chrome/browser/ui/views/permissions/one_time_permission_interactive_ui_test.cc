// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/one_time_permission_provider.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

namespace {

std::string RunScript(content::RenderFrameHost* render_frame_host,
                      const std::string& script) {
  return content::EvalJs(render_frame_host, script).ExtractString();
}

class IndicatorObserver : public MediaStreamCaptureIndicator::Observer {
 public:
  IndicatorObserver() {
    MediaCaptureDevicesDispatcher::GetInstance()
        ->GetMediaStreamCaptureIndicator()
        ->AddObserver(this);
  }

  ~IndicatorObserver() override {
    MediaCaptureDevicesDispatcher::GetInstance()
        ->GetMediaStreamCaptureIndicator()
        ->RemoveObserver(this);
  }

  void Wait() { loop_.Run(); }

  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override {
    loop_.Quit();
  }

  base::RunLoop loop_;
};

}  // namespace

class OneTimePermissionInteractiveUiTest : public WebRtcTestBase {
 public:
  OneTimePermissionInteractiveUiTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(6.66, 9.99)) {
    feature_list_.InitWithFeatures({permissions::features::kOneTimePermission},
                                   {});
  }

  OneTimePermissionInteractiveUiTest(
      const OneTimePermissionInteractiveUiTest&) = delete;
  OneTimePermissionInteractiveUiTest& operator=(
      const OneTimePermissionInteractiveUiTest&) = delete;

  ~OneTimePermissionInteractiveUiTest() override = default;

  enum InitializationOptions {
    // The default profile and browser window will be used.
    INITIALIZATION_DEFAULT,

    // A new tab will be created using the default profile and browser window.
    INITIALIZATION_NEWTAB,

    // A new tab will be created using the default profile and browser window,
    // then the tab at position 0 will be closed.
    INITIALIZATION_CLOSETAB_NEWTAB
  };

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* current_browser() { return current_browser_; }

  GURL GetDifferentOriginUrl() const { return GURL("https://test.com"); }

  GURL GetGeolocationGurl() const {
    return embedded_test_server()->GetURL("/geolocation/simple.html");
  }

  GURL GetWebrtcGurl() const {
    return embedded_test_server()->GetURL("/webrtc/webrtc_jsep01_test.html");
  }

  // Initializes the test server and navigates to the initial url.
  void Initialize(InitializationOptions options, const GURL& url) {
    current_browser_ = browser();
    if (options == INITIALIZATION_NEWTAB) {
      chrome::NewTab(current_browser_);
    } else if (options == INITIALIZATION_CLOSETAB_NEWTAB) {
      chrome::NewTabToRight(current_browser_);
      current_browser_->tab_strip_model()->CloseWebContentsAt(
          0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    }
    ASSERT_TRUE(current_browser_);
    ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        current_browser_, url, 1));

    SetFrameForScriptExecutionToCurrent(
        current_browser_->tab_strip_model()->GetActiveWebContents());
  }

  void SetFrameForScriptExecutionToCurrent(content::WebContents* contents) {
    render_frame_host_ = contents->GetPrimaryMainFrame();
  }

  void CloseLastLocalStreamAt(int index) {
    IndicatorObserver observer;
    WebRtcTestBase::CloseLastLocalStream(
        current_browser()->tab_strip_model()->GetWebContentsAt(index));
    observer.Wait();
  }

  void FireRunningExpirationTimers() {
    OneTimePermissionsTrackerFactory::GetForBrowserContext(
        current_browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetBrowserContext())
        ->FireRunningTimersForTesting();
  }

  void WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::AutoResponseType auto_response,
      bool expect_prompt) {
    content::WebContents* contents =
        current_browser()->tab_strip_model()->GetActiveWebContents();
    SetFrameForScriptExecutionToCurrent(contents);
    permissions::PermissionRequestManager::FromWebContents(contents)
        ->set_auto_response_for_test(auto_response);
    permissions::PermissionRequestObserver observer(contents);

    if (expect_prompt) {
      // Control will return as soon as the API call is made, and then the
      // observer will wait for the request to display.
      std::string result =
          content::EvalJs(contents, "geoStartWithAsyncResponse();")
              .ExtractString();
      if (auto_response == permissions::PermissionRequestManager::DISMISS ||
          auto_response == permissions::PermissionRequestManager::DENY_ALL) {
        EXPECT_TRUE(result == "request-callback-error");
      } else {
        EXPECT_TRUE(
            result == "request-callback-success" ||  // First request.
            result ==
                "geoposition-updated");  // May occur when page is not reloaded,
                                         // lost permission and successfully
                                         // prompted for it again.
      }
      observer.Wait();
    } else {
      // Control will return once one of the callbacks fires.
      RunScript(render_frame_host_, "geoStartWithSyncResponse()");
    }

    EXPECT_EQ(expect_prompt, observer.request_shown());
  }

  void GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::AutoResponseType auto_response,
      bool expect_prompt,
      int tab_index = 0) {
    content::WebContents* contents =
        current_browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    SetFrameForScriptExecutionToCurrent(contents);
    permissions::PermissionRequestManager::FromWebContents(contents)
        ->set_auto_response_for_test(auto_response);
    permissions::PermissionRequestObserver observer(contents);
    GetUserMedia(contents, kAudioVideoCallConstraints);
    EXPECT_EQ(expect_prompt, observer.request_shown());
    EXPECT_EQ(
        content::EvalJs(render_frame_host_, "obtainGetUserMediaResult();"),
        kOkGotStream);
  }

  void DiscardTabAt(int index) {
    resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
        browser()->tab_strip_model()->GetWebContentsAt(index))
        ->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);
  }

 protected:
  void OtpEventExpectUniqueSample(ContentSettingsType content_setting_type,
                                  permissions::OneTimePermissionEvent event,
                                  int occ) {
    histograms_.ExpectUniqueSample(
        permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
            content_setting_type),
        static_cast<base::HistogramBase::Sample>(event), occ);
  }

  void OtpEventExpectBucketCount(ContentSettingsType content_setting_type,
                                 permissions::OneTimePermissionEvent event,
                                 int occ) {
    histograms_.ExpectBucketCount(
        permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
            content_setting_type),
        static_cast<base::HistogramBase::Sample>(event), occ);
  }

  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  raw_ptr<Browser, AcrossTasksDanglingUntriaged> current_browser_ = nullptr;

  base::HistogramTester histograms_;

 private:
  // The render frame host where JS calls will be executed.
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      render_frame_host_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       OneTimeGrantResetsEmbargoCounter) {
  auto* autoblocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser()
              ->tab_strip_model()
              ->GetWebContentsAt(0)
              ->GetBrowserContext());
  EXPECT_EQ(0, autoblocker->GetDismissCount(GetGeolocationGurl(),
                                            ContentSettingsType::GEOLOCATION));

  // Navigate to geolocation page, request geolocation permission, expect prompt
  // and dismiss it.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::DISMISS, true);

  // Expect that embargo counter has been incremented.
  EXPECT_EQ(1, autoblocker->GetDismissCount(GetGeolocationGurl(),
                                            ContentSettingsType::GEOLOCATION));

  // Refresh.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it once.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Expect the embargo counter has been reset to 0.
  EXPECT_EQ(0, autoblocker->GetDismissCount(GetGeolocationGurl(),
                                            ContentSettingsType::GEOLOCATION));
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       SameTabForegroundBehaviour) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Navigate to the same site in the same tab.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Ensure position is accessible in new tab without prompt.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false);

  // Ensure grant event is recorded
  OtpEventExpectUniqueSample(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       VerifyDifferentTabBehaviour) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Open new tab to the right.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_NEWTAB, GetGeolocationGurl()));

  // Ensure position is accessible in new tab without prompt.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false);

  // Ensure grant event is only recorded once
  OtpEventExpectUniqueSample(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       VerifyPermissionPromptAfterClosingAllTabsToOrigin) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  //  Ensure no content setting is persisted.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_CLOSETAB_NEWTAB, GetGeolocationGurl()));

  // Ensure that a prompt is triggered.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ALL, true);

  // Since the second request was resolved with a persistent accept, only one
  // otp-grant event should be recorded
  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);

  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::ALL_TABS_CLOSED_OR_DISCARDED, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       DiscardingTabToOriginRevokesPermission) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Open new tab to the right.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_NEWTAB, GetDifferentOriginUrl()));

  // Discard previous tab
  DiscardTabAt(0);

  // Open new tab to the right.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Ensure that a prompt is triggered again when requesting geolocation
  // permission.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ALL, true);

  // Since the second request was resolved with a persistent accept, only one
  // otp-grant event should be recorded
  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);

  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::ALL_TABS_CLOSED_OR_DISCARDED, 1);
}

IN_PROC_BROWSER_TEST_F(
    OneTimePermissionInteractiveUiTest,
    DiscardingTabToOriginDoesNotRevokePermissionIfDifferentTabToOriginIsUsingIt) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Open new tab to the right.
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_NEWTAB, GetGeolocationGurl()));

  // Request geolocation permission in new tab, expect no prompt.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false);

  // Discard previous tab
  DiscardTabAt(0);

  // Reload
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission and ensure that no prompt is triggered.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false);

  // Ensure that only a single GRANTED_ONE_TIME event is recorded
  OtpEventExpectUniqueSample(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       GeolocationIsRevokedAfterFiveMinutesInBackground) {
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_DEFAULT, GetGeolocationGurl()));

  // Request geolocation permission, expect prompt and grant it.
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Open new tab, this puts the first tab in the background
  ASSERT_NO_FATAL_FAILURE(
      Initialize(INITIALIZATION_NEWTAB, GetDifferentOriginUrl()));

  // Fire running timers. This means, that all one time permission expiration
  // timers that are running at this point in time will fire their callbacks and
  // are stopped.
  OneTimePermissionsTrackerFactory::GetForBrowserContext(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetBrowserContext())
      ->FireRunningTimersForTesting();

  // Go to previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Ensure that a prompt is triggered again when requesting permission
  WatchPositionAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 2);

  OtpEventExpectBucketCount(
      ContentSettingsType::GEOLOCATION,
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       CamMicNotRevokedWhenPausedInForeground) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  // Request cam/mic permission, expect prompt and grant it.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true, 0);

  // Stop local streams (i.e. stop all cam/mic permission usage within specified
  // web contents).
  CloseLastLocalStreamAt(0);

  // Fire running timers. This means, that all one time permission
  // expiration timers that are running at this point in time will fire
  // their callbacks and are stopped.
  FireRunningExpirationTimers();

  // Request cam/mic permission, expect no prompt is triggered.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false, 0);

  // Ensure that only a single GRANTED_ONE_TIME event is recorded per permission
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_MIC,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       CamMicRevokedWhenPausedInBackground) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  // Request cam/mic permission, expect prompt and grant it.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true, 0);

  // Open new tab, this puts the first tab in the background.
  Initialize(INITIALIZATION_NEWTAB, GetDifferentOriginUrl());

  // Stop local streams in previous tab (i.e. stop all cam/mic permission usage
  // within specified web contents).
  CloseLastLocalStreamAt(0);

  // Fire running timers. This means, that all one time permission
  // expiration timers that are running at this point in time will fire
  // their callbacks and are stopped.
  FireRunningExpirationTimers();

  // Switch back to previous tab
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Request cam/mic permission, expect a prompt is triggered.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true, 0);

  OtpEventExpectBucketCount(
      ContentSettingsType::MEDIASTREAM_MIC,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 2);

  OtpEventExpectBucketCount(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 2);

  OtpEventExpectBucketCount(
      ContentSettingsType::MEDIASTREAM_MIC,
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND, 1);
  OtpEventExpectBucketCount(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       CamMicDoesNotExpireWhenNotPausedInBackground) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  // Request cam/mic permission, expect prompt and grant it.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true, 0);

  // Open new tab, this puts the first tab in the background.
  Initialize(INITIALIZATION_NEWTAB, GetDifferentOriginUrl());

  // Fire running timers. This means, that all one time permission expiration
  // timers that are running at this point in time will fire their callbacks and
  // are stopped.
  FireRunningExpirationTimers();

  // Switch back to previous tab
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Request cam/mic permission, expect no prompt is triggered.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false, 0);

  // Ensure that only a single GRANTED_ONE_TIME event is recorded per permission
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_MIC,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

IN_PROC_BROWSER_TEST_F(OneTimePermissionInteractiveUiTest,
                       CamMicDoesNotExpireWhenPausedInForeground) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  // Request cam/mic permission, expect prompt and grant it.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true, 0);

  // Stop local streams (i.e. stop all cam/mic permission usage within specified
  // web contents).
  CloseLastLocalStreamAt(0);

  // Fire running timers. This means, that all one time permission
  // expiration timers that are running at this point in time will fire
  // their callbacks and are stopped.
  FireRunningExpirationTimers();

  // Request cam/mic permission, expect no prompt is triggered.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false, 0);

  // Ensure that only a single GRANTED_ONE_TIME event is recorded per permission
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_MIC,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
  OtpEventExpectUniqueSample(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME, 1);
}

class OneTimePermissionExpiryEnforcementUmaInteractiveUiTest
    : public OneTimePermissionInteractiveUiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OneTimePermissionExpiryEnforcementUmaInteractiveUiTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {content_settings::features::kActiveContentSettingExpiry}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {content_settings::features::kActiveContentSettingExpiry});
    }
  }
  OneTimePermissionExpiryEnforcementUmaInteractiveUiTest(
      const OneTimePermissionExpiryEnforcementUmaInteractiveUiTest&) = delete;
  OneTimePermissionExpiryEnforcementUmaInteractiveUiTest& operator=(
      const OneTimePermissionExpiryEnforcementUmaInteractiveUiTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OneTimePermissionExpiryEnforcementUmaInteractiveUiTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(OneTimePermissionExpiryEnforcementUmaInteractiveUiTest,
                       TestExpiryEnforcement) {
  base::HistogramTester histograms;
  const std::string kActiveExpiryHistogram =
      "ContentSettings.ActiveExpiry.OneTimePermissionProvider."
      "ContentSettingsType";

  bool active_expiry_is_active = GetParam();
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // Setup to request content setting in the past (maximum one time
  // permission grant lifetime)
  base::Time now(base::Time::Now());
  base::Time past(now - permissions::kOneTimePermissionMaximumLifetime);
  base::SimpleTestClock clock;
  clock.SetNow(past);
  hcsm->SetClockForTesting(&clock);

  // Request permission, expect prompt and grant it.
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, true);

  // Manually run expiry enforcement
  hcsm->DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
      ContentSettingsType::MEDIASTREAM_MIC);
  hcsm->DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
      ContentSettingsType::MEDIASTREAM_CAMERA);

  // Reload and ensure permission remains accessible without prompt.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, GetWebrtcGurl()));

  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE, false);

  // Set clock time to now (i.e. 16 hours later) and then manually run expiry
  // enforcement again.
  clock.SetNow(now + base::Seconds(5));
  hcsm->DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
      ContentSettingsType::MEDIASTREAM_MIC);
  hcsm->DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
      ContentSettingsType::MEDIASTREAM_CAMERA);

  // Ensure a request without reload triggers a prompt again only if active
  // expiry is active
  GetUserMediaAndExpectGrantedPermission(
      permissions::PermissionRequestManager::ACCEPT_ONCE,
      active_expiry_is_active);

  // Check UMA records for expiry events (only recorded if active expiry is
  // enabled)
  histograms.ExpectTotalCount(kActiveExpiryHistogram,
                              active_expiry_is_active ? 2 : 0);
  histograms.ExpectBucketCount(
      kActiveExpiryHistogram,
      static_cast<base::HistogramBase::Sample>(
          content_settings_uma_util::ContentSettingTypeToHistogramValue(
              ContentSettingsType::MEDIASTREAM_MIC)),
      active_expiry_is_active ? 1 : 0);
  histograms.ExpectBucketCount(
      kActiveExpiryHistogram,
      static_cast<base::HistogramBase::Sample>(
          content_settings_uma_util::ContentSettingTypeToHistogramValue(
              ContentSettingsType::MEDIASTREAM_CAMERA)),
      active_expiry_is_active ? 1 : 0);

  // Check UMA records for grant events (if expiry is disabled, there's only one
  // grant event)
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      active_expiry_is_active ? 3 : 1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      active_expiry_is_active ? 2 : 1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::EXPIRED_AFTER_MAXIMUM_LIFETIME),
      active_expiry_is_active ? 1 : 0);

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      active_expiry_is_active ? 3 : 1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      active_expiry_is_active ? 2 : 1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::EXPIRED_AFTER_MAXIMUM_LIFETIME),
      active_expiry_is_active ? 1 : 0);

  hcsm->SetClockForTesting(base::DefaultClock::GetInstance());
}
