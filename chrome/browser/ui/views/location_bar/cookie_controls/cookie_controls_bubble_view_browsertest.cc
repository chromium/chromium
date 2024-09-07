// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/test/widget_test.h"

class CookieControlsBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  CookieControlsBubbleViewBrowserTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();

    // Block 3PC and navigate to a page which accesses 3PC, to ensure entry
    // point is available.
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), third_party_cookie_page_url()));

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        TrackingProtectionSettingsFactory::GetForProfile(browser()->profile()));

    coordinator_ = std::make_unique<CookieControlsBubbleCoordinator>();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());

    // If the test did not close the bubble, close and wait for it here.
    WaitForBubbleClose();

    coordinator_ = nullptr;
    controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void ShowBubble() {
    coordinator_->ShowBubble(active_web_contents(), controller_.get());
  }

  void WaitForBubbleClose() {
    if (bubble_view()) {
      views::test::WidgetDestroyedWaiter waiter(bubble_view()->GetWidget());
      bubble_view()->GetWidget()->Close();
      waiter.Wait();
      EXPECT_EQ(coordinator_->GetBubble(), nullptr);
    }
  }

  void SimulateTogglePress(bool new_value) {
    view_controller()->OnToggleButtonPressed(new_value);
  }

  void CheckCookiesException(const GURL& first_party_url, bool should_exist) {
    content_settings::SettingInfo info;
    EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                  GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
              CONTENT_SETTING_ALLOW);
    EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
    // If an exception exists, it will have a targeted secondary pattern.
    // If it does not exist, it will fall through the default wildcard.
    if (should_exist) {
      EXPECT_EQ(info.secondary_pattern,
                ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    first_party_url));
    } else {
      EXPECT_TRUE(info.secondary_pattern.MatchesAllHosts());
    }
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  GURL third_party_cookie_page_url() {
    return https_server()->GetURL("a.test",
                                  "/third_party_partitioned_cookies.html");
  }
  CookieControlsBubbleViewImpl* bubble_view() {
    return coordinator_->GetBubble();
  }
  CookieControlsBubbleViewController* view_controller() {
    return coordinator_->GetViewControllerForTesting();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }
  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<CookieControlsBubbleCoordinator> coordinator_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest,
                       ToggleCreatesCookiesException) {
  ShowBubble();
  CheckCookiesException(third_party_cookie_page_url(), /*should_exist=*/false);

  SimulateTogglePress(true);
  CheckCookiesException(third_party_cookie_page_url(), /*should_exist=*/true);

  SimulateTogglePress(false);
  CheckCookiesException(third_party_cookie_page_url(), /*should_exist=*/false);
}

class TrackingProtectionBubbleViewBrowserTest
    : public CookieControlsBubbleViewBrowserTest {
 public:
  TrackingProtectionBubbleViewBrowserTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // Enable FPP to display UB UX with ACT features
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kFingerprintingProtectionUserBypass}, {});
  }

  ContentSetting GetTrackingProtectionSetting() {
    return host_content_settings_map()->GetContentSetting(
        GURL(), third_party_cookie_page_url(),
        ContentSettingsType::TRACKING_PROTECTION);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(TrackingProtectionBubbleViewBrowserTest,
                       ToggleCreatesTrackingProtectionException) {
  ShowBubble();
  EXPECT_EQ(GetTrackingProtectionSetting(), CONTENT_SETTING_BLOCK);
  SimulateTogglePress(false);
  EXPECT_EQ(GetTrackingProtectionSetting(), CONTENT_SETTING_ALLOW);
  SimulateTogglePress(true);
  EXPECT_EQ(GetTrackingProtectionSetting(), CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest,
                       HidingControlsClosesBubble) {
  ShowBubble();
  views::test::WidgetDestroyedWaiter waiter(bubble_view()->GetWidget());
  view_controller()->OnStatusChanged(
      /*controls_visible=*/false,
      /*protections_on=*/false, CookieControlsEnforcement::kNoEnforcement,
      CookieBlocking3pcdStatus::kNotIn3pcd, base::Time(), /*features=*/{});
  waiter.Wait();
  EXPECT_EQ(bubble_view(), nullptr);
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest, PageReload) {
  // Confirm that a navigation to the same page is triggered when the user
  // changes the cookie setting and the bubble is closed.
  ShowBubble();

  testing::NiceMock<content::MockWebContentsObserver> observer(
      active_web_contents());

  EXPECT_CALL(observer, DidStartNavigation(testing::Truly(
                            [&](content::NavigationHandle* navigation_handle) {
                              return navigation_handle->GetURL() ==
                                     third_party_cookie_page_url();
                            })));

  SimulateTogglePress(true);

  bubble_view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  EXPECT_TRUE(bubble_view()->GetReloadingView()->GetVisible());

  WaitForBubbleClose();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest,
                       PageReload_CloseView) {
  // Check that the user is able to close the reloading view.
  ShowBubble();

  // Manually show the reloading view to avoid any races with page refresh.
  bubble_view()->SwitchToReloadingView();

  bubble_view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  EXPECT_TRUE(bubble_view()->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest,
                       NoPageReload_NoChange) {
  // Confirm that no navigation is triggered if no change to effective state is
  // made.
  ShowBubble();

  testing::NiceMock<content::MockWebContentsObserver> observer(
      active_web_contents());

  EXPECT_CALL(observer, DidStartNavigation(testing::_)).Times(0);

  SimulateTogglePress(true);
  SimulateTogglePress(false);

  bubble_view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);

  WaitForBubbleClose();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewBrowserTest,
                       NoPageReload_NonUserClose) {
  // The page should only be reloaded if the close reason is related to a
  // user action, and not a direct call to close.
  ShowBubble();

  testing::NiceMock<content::MockWebContentsObserver> observer(
      active_web_contents());

  EXPECT_CALL(observer, DidStartNavigation(testing::_)).Times(0);

  SimulateTogglePress(true);

  bubble_view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);

  WaitForBubbleClose();
}
