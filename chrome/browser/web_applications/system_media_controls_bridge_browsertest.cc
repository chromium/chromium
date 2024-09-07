// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_MEDIA_CONTROLS_BRIDGE_BROWSERTEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_MEDIA_CONTROLS_BRIDGE_BROWSERTEST_H_

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/system_media_controls/system_media_controls.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/system_media_controls_bridge_test_utils.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_media_controls {
namespace testing {

// Runs on macOS only. Tests SystemMedia
class SystemMediaControlsBridgeBrowsertest
    : public web_app::WebAppBrowserTestBase {
 public:
  SystemMediaControlsBridgeBrowsertest() {
    feature_list_.InitWithFeatures({features::kWebAppSystemMediaControls}, {});
  }

  SystemMediaControlsBridgeBrowsertest(
      const SystemMediaControlsBridgeBrowsertest&) = delete;
  SystemMediaControlsBridgeBrowsertest& operator=(
      const SystemMediaControlsBridgeBrowsertest&) = delete;

  ~SystemMediaControlsBridgeBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Start the test server so we can use the test media session pages.
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server()->Start());

    wait_for_bridge_creation_run_loop_.emplace();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void MaybeWaitForAppShimConnection(AppShimHost* app_shim_host) {
    // If the app shim has not yet connected, set the callback for testing.
    if (!app_shim_host->HasBootstrapConnected()) {
      base::test::TestFuture<void> future;
      app_shim_host->SetOnShimConnectedForTesting(future.GetCallback());
      EXPECT_TRUE(future.Wait());
    }
  }

  void TearDownOnMainThread() override {
    system_media_controls::SystemMediaControls::
        SetVisibilityChangedCallbackForTesting(nullptr);
  }

  void StartPlaybackAndWaitForStart(Browser* browser,
                                    const std::string& media_id) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    // Play the given media
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(content::JsReplace(
            "document.getElementById($1).play();", media_id)),
        base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);

    // Wait for start
    content::MediaStartStopObserver observer(
        web_contents, content::MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop(Browser* browser, const std::string& id) {
    if (!IsPlaying(browser, id)) {
      return;
    }
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();

    content::MediaStartStopObserver observer(
        web_contents, content::MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  bool IsPlaying(Browser* browser, const std::string& id) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, content::JsReplace(
                                    "!document.getElementById($1).paused;", id))
        .ExtractBool();
  }

  void SetUpOnBridgeCreatedCallback(bool for_web_app) {
    auto on_bridge_created = base::BindLambdaForTesting([&]() {
      num_bridges_created_++;
      wait_for_bridge_creation_run_loop_->Quit();
    });
    if (for_web_app) {
      content::SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
          std::move(on_bridge_created));
    } else {
      content::SetOnBrowserSystemMediaControlsBridgeCreatedCallbackForTesting(
          std::move(on_bridge_created));
    }
  }

 protected:
  std::optional<base::RunLoop> wait_for_bridge_creation_run_loop_;
  int num_bridges_created_ = 0;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest, TwoApps) {
  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // At this point, WebAppSystemMediaControlsManager exists,
  // but no SystemMediaControls have been made yet. Before playing media, tell
  // the Manager where to call us back once it starts making SMCs/Bridges
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/true);

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser1, "long-video-loop");

  // Wait for the SystemMediaControlsBridge to be created.
  EXPECT_EQ(num_bridges_created_, 0);
  wait_for_bridge_creation_run_loop_->Run();
  // Verify that 1 bridge has been created.
  EXPECT_EQ(num_bridges_created_, 1);
  wait_for_bridge_creation_run_loop_.emplace();  // Reset run loop for reuse.

  // Install and launch a different test media session PWA.
  webapps::AppId app_id2 = InstallPWA(https_server()->GetURL(
      "/media/session/media_controls/media-session2.html"));
  Browser* web_app_browser2 = LaunchWebAppBrowserAndWait(app_id2);
  EXPECT_TRUE(web_app_browser2);

  // Wait for 2nd app shim to connect.
  AppShimHost* app_shim_host2 =
      app_shim_manager->FindHost(web_app_browser2->profile(), app_id2);
  MaybeWaitForAppShimConnection(app_shim_host2);

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser2, "short-video-loop");

  // Wait for the 2nd SystemMediaControlsBridge to be created.
  wait_for_bridge_creation_run_loop_->Run();
  // Verify that 2 bridges have been created.
  EXPECT_EQ(num_bridges_created_, 2);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest, OneBrowser) {
  // Set up the browser SMCBridge listener.
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/false);

  // Navigate the browser to the test media page.
  NavigateParams params(
      browser(), https_server()->GetURL("/media/session/media-session.html"),
      ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  StartPlaybackAndWaitForStart(browser(), "long-video-loop");

  // Wait for the browser's SystemMediaControlsBridge to be made.
  EXPECT_EQ(num_bridges_created_, 0);
  wait_for_bridge_creation_run_loop_->Run();
  EXPECT_EQ(num_bridges_created_, 1);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest, OneBrowserOneApp) {
  // Set up the browser SMCBridge listener.
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/false);

  // Navigate the browser to the test media page.
  NavigateParams params(
      browser(), https_server()->GetURL("/media/session/media-session.html"),
      ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  StartPlaybackAndWaitForStart(browser(), "long-video-loop");

  // Wait for the browser's SystemMediaControlsBridge to be made.
  EXPECT_EQ(num_bridges_created_, 0);
  wait_for_bridge_creation_run_loop_->Run();
  EXPECT_EQ(num_bridges_created_, 1);
  wait_for_bridge_creation_run_loop_.emplace();

  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // At this point, WebAppSystemMediaControlsManager exists,
  // but no SystemMediaControls have been made yet. Before playing media, tell
  // the Manager where to call us back once it starts making SMCs/Bridges
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/true);

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser1, "long-video-loop");

  // Wait for the SystemMediaControlsBridge to be created.
  EXPECT_EQ(num_bridges_created_, 1);  // Should be 1 because of browser bridge
  wait_for_bridge_creation_run_loop_->Run();
  // Verify that another bridge has been created.
  EXPECT_EQ(num_bridges_created_, 2);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest, DuplicateApp) {
  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // At this point, WebAppSystemMediaControlsManager exists,
  // but no SystemMediaControls have been made yet. Before playing media, tell
  // the Manager where to call us back once it starts making SMCs/Bridges
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/true);

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser1, "long-video-loop");

  // Wait for the SystemMediaControlsBridge to be created.
  EXPECT_EQ(num_bridges_created_, 0);
  wait_for_bridge_creation_run_loop_->Run();

  // Verify that 1 bridge got made.
  EXPECT_EQ(num_bridges_created_, 1);
  wait_for_bridge_creation_run_loop_.emplace();  // Reset run loop for reuse.

  // Launch THE SAME test media session PWA.
  Browser* web_app_browser2 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser2);

  // We don't need to wait for the app shim connection here because duplicate
  // apps share 1 app shim.

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser2, "long-video-loop");

  // Verify that still only 1 bridge got made, as duplicate apps also share 1
  // SMCBridge.
  EXPECT_EQ(num_bridges_created_, 1);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest,
                       CommandQuitOneApp) {
  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // Start playing the audio.
  StartPlaybackAndWaitForStart(web_app_browser1, "long-video-loop");

  // Simulate Command+Q to quit the app
  content::SimulateKeyPress(
      web_app_browser1->tab_strip_model()->GetActiveWebContents(),
      ui::DomKey::FromCharacter('Q'), ui::DomCode::US_Q, ui::VKEY_Q, false,
      false, false, /*command=*/true);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest,
                       NowPlayingInfoHiddenOnNavigationAway) {
  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // Register for a callback when the bridge is made. We don't really care about
  // the bridge itself here, but this ensures everything will be set up.
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/true);

  // Start the media session.
  StartPlaybackAndWaitForStart(web_app_browser1, "long-video-loop");

  // Wait for the SystemMediaControlsBridge to be created.
  wait_for_bridge_creation_run_loop_->Run();

  // Set up the listener so the app shim can tell us when the visibility of the
  // now playing info changes.
  base::RunLoop wait_for_visibility_change;
  bool new_visibility = true;
  auto visibility_changed_callback =
      base::BindLambdaForTesting([&](bool is_visible) {
        new_visibility = is_visible;
        wait_for_visibility_change.Quit();
      });
  // Register the callback.
  system_media_controls::SystemMediaControls::
      SetVisibilityChangedCallbackForTesting(&visibility_changed_callback);

  // Check the pwa is still playing, and navigate away to a different url.
  EXPECT_TRUE(IsPlaying(web_app_browser1, "long-video-loop"));
  GURL http_url2(https_server()->GetURL("/media/session/title1.html"));
  NavigateParams params(web_app_browser1, http_url2, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // Start the visibility changed run loop and wait for the app to tell us
  // that the metadata has been cleared, therefore the controls should not be
  // visible.
  wait_for_visibility_change.Run();
  EXPECT_FALSE(new_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemMediaControlsBridgeBrowsertest,
                       NowPlayingInfoHiddenOnAudioEnd) {
  // Set up a media session in 1 PWA.
  // Install and launch a test media session PWA.
  webapps::AppId app_id1 =
      InstallPWA(https_server()->GetURL("/media/session/media-session.html"));
  Browser* web_app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  EXPECT_TRUE(web_app_browser1);

  // Wait for the app shim to connect.
  apps::AppShimManager* app_shim_manager = apps::AppShimManager::Get();
  AppShimHost* app_shim_host =
      app_shim_manager->FindHost(web_app_browser1->profile(), app_id1);
  MaybeWaitForAppShimConnection(app_shim_host);

  // Register for a callback when the bridge is made. We don't really care about
  // the bridge itself here, but this ensures everything will be set up.
  SetUpOnBridgeCreatedCallback(/*for_web_app=*/true);

  // Start the media session and wait for the controls to become visible.
  StartPlaybackAndWaitForStart(web_app_browser1, "short-video");

  // Wait for the SystemMediaControlsBridge to be created.
  wait_for_bridge_creation_run_loop_->Run();

  // Set up the listener so the app shim can tell us when the visibility of the
  // now playing info changes.
  base::RunLoop wait_for_visibility_change;
  bool new_visibility = true;
  auto visibility_changed_callback =
      base::BindLambdaForTesting([&](bool is_visible) {
        new_visibility = is_visible;
        wait_for_visibility_change.Quit();
      });
  // Register the callback.
  system_media_controls::SystemMediaControls::
      SetVisibilityChangedCallbackForTesting(&visibility_changed_callback);

  // Wait for the audio track to end on its own.
  WaitForStop(web_app_browser1, "short-video");
  EXPECT_FALSE(IsPlaying(web_app_browser1, "short-video"));

  // Start the visibility changed run loop and wait for the app to tell us
  // that the metadata has been cleared, therefore the controls should not be
  // visible.
  wait_for_visibility_change.Run();
  EXPECT_FALSE(new_visibility);
}

}  // namespace testing
}  // namespace system_media_controls

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_MEDIA_CONTROLS_BRIDGE_MAC_BROWSERTEST_H_
