// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
}

class GeolocationSwitchInteractiveTest : public InteractiveBrowserTest {
 public:
  GeolocationSwitchInteractiveTest()
      : https_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    scoped_features_.InitWithFeatures({ash::features::kCrosPrivacyHub}, {});
  }

  GeolocationSwitchInteractiveTest(const GeolocationSwitchInteractiveTest&) =
      delete;
  void operator=(const GeolocationSwitchInteractiveTest&) = delete;

  ~GeolocationSwitchInteractiveTest() override = default;

  // InteractiveBrowserTest:
  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  net::EmbeddedTestServer* https_server() const { return https_server_.get(); }

  GURL GetURL() const {
    return https_server()->GetURL("a.test", "/geolocation/simple.html");
  }

  void SetBrowserPermission(ContentSetting setting) {
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetContentSettingDefaultScope(
            GetURL(), GetURL(), ContentSettingsType::GEOLOCATION, setting);
  }

  void SetSystemPermission(ash::GeolocationAccessLevel access_level) {
    ash::GeolocationPrivacySwitchController::Get()->SetAccessLevel(
        access_level);
  }

  void ExpectAndApproveBrowserPrompt(bool should_there_be_a_prompt) {
    if (should_there_be_a_prompt) {
      // There should be a prompt and we are going to approve
      RunTestSequence(
          InstrumentTab(kWebContentsElementId),
          NavigateWebContents(kWebContentsElementId, GetURL()),
          CheckJsResult(kWebContentsElementId, "geoStartWithSyncResponse",
                        "requested"),
          WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
          WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

          PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
          WaitForHide(PermissionPromptBubbleBaseView::kMainViewId));
    } else {
      // There should be no prompt.
      RunTestSequence(
          InstrumentTab(kWebContentsElementId),
          NavigateWebContents(kWebContentsElementId, GetURL()),
          CheckJsResult(kWebContentsElementId, "geoStartWithSyncResponse",
                        "requested"),
          EnsureNotPresent(PermissionPromptBubbleBaseView::kMainViewId));
    }
  }

  void ExpectOSNotification(bool should_there_be_a_notification) {
    const message_center::Notification* notification_ptr =
        message_center::MessageCenter::Get()->FindNotificationById(
            ash::PrivacyHubNotificationController::
                kGeolocationSwitchNotificationId);
    if (should_there_be_a_notification) {
      EXPECT_TRUE(notification_ptr);
    } else {
      EXPECT_FALSE(notification_ptr);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserBlockSystemAllow) {
  SetBrowserPermission(CONTENT_SETTING_BLOCK);
  SetSystemPermission(ash::GeolocationAccessLevel::kAllowed);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserBlockSystemOnly) {
  SetBrowserPermission(CONTENT_SETTING_BLOCK);
  SetSystemPermission(ash::GeolocationAccessLevel::kDisallowed);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}

// TODO(b/312485657): Enable the testcase.
IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserBlockSystemBlock) {
  SetBrowserPermission(CONTENT_SETTING_BLOCK);
  SetSystemPermission(ash::GeolocationAccessLevel::kDisallowed);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserAskSystemAllow) {
  SetBrowserPermission(CONTENT_SETTING_ASK);
  SetSystemPermission(ash::GeolocationAccessLevel::kAllowed);

  ExpectAndApproveBrowserPrompt(true);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest, BrowserAskSystemOnly) {
  SetBrowserPermission(CONTENT_SETTING_ASK);
  SetSystemPermission(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);

  ExpectAndApproveBrowserPrompt(true);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserAskSystemBlock) {
  SetBrowserPermission(CONTENT_SETTING_ASK);
  SetSystemPermission(ash::GeolocationAccessLevel::kDisallowed);

  ExpectAndApproveBrowserPrompt(true);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserAllowSystemAllow) {
  SetBrowserPermission(CONTENT_SETTING_ALLOW);
  SetSystemPermission(ash::GeolocationAccessLevel::kAllowed);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserAllowSystemOnly) {
  SetBrowserPermission(CONTENT_SETTING_ALLOW);
  SetSystemPermission(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationSwitchInteractiveTest,
                       BrowserAllowSystemBlock) {
  SetBrowserPermission(CONTENT_SETTING_ALLOW);
  SetSystemPermission(ash::GeolocationAccessLevel::kDisallowed);

  ExpectAndApproveBrowserPrompt(false);
  ExpectOSNotification(false);
}
