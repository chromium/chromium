// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#endif

namespace web_app {

class WebAppNotificationsBrowserTest : public WebAppBrowserTestBase {
 public:
  using WebAppBrowserTestBase::WebAppBrowserTestBase;
  ~WebAppNotificationsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    display_service_tester_.reset();
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  NotificationDisplayServiceTester& display_service_tester() {
    DCHECK(display_service_tester_);
    return *display_service_tester_;
  }

  void SetAppBrowserForAppId(const webapps::AppId& app_id) {
    Browser* app_browser = FindWebAppBrowser(profile(), app_id);
    ASSERT_TRUE(app_browser);
    app_browser_ = app_browser;
  }

  Browser& app_browser() {
    DCHECK(app_browser_);
    return *app_browser_;
  }

  content::WebContents* GetActiveWebContents() {
    return app_browser().tab_strip_model()->GetActiveWebContents();
  }

  content::EvalJsResult AwaitScript(const std::string& script) {
    content::EvalJsResult js_result =
        content::EvalJs(GetActiveWebContents()->GetPrimaryMainFrame(), script,
                        content::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

    // Purges all pending messages to propagate them to notification views.
    // Also prevents content::PlatformNotificationServiceProxy from crashing.
    content::RunAllTasksUntilIdle();
    return js_result;
  }

#if BUILDFLAG(IS_MAC)
  std::string RequestAndRespondToPermission(
      mac_notifications::mojom::RequestPermissionResult os_response,
      permissions::PermissionRequestManager::AutoResponseType bubble_response =
          permissions::PermissionRequestManager::NONE) {
    apps::AppShimManager::Get()->SetNotificationPermissionResponseForTesting(
        os_response);
    content::WebContents* web_contents = GetActiveWebContents();
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(bubble_response);

    return AwaitScript("requestPermission()").ExtractString();
  }
#else
  std::string RequestAndRespondToPermission(
      permissions::PermissionRequestManager::AutoResponseType bubble_response) {
    content::WebContents* web_contents = GetActiveWebContents();
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(bubble_response);

    return AwaitScript("requestPermission()").ExtractString();
  }
#endif

  bool RequestAndAcceptPermission() {
    return "granted" == RequestAndRespondToPermission(
#if BUILDFLAG(IS_MAC)
                            mac_notifications::mojom::RequestPermissionResult::
                                kPermissionGranted,
#endif
                            permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  // Returns a vector with the Notification objects that are being displayed
  // by the notification display service. Synchronous.
  std::vector<message_center::Notification> GetDisplayedNotifications(
      bool is_persistent) const {
    NotificationHandler::Type type =
        is_persistent ? NotificationHandler::Type::WEB_PERSISTENT
                      : NotificationHandler::Type::WEB_NON_PERSISTENT;

    return display_service_tester_->GetDisplayedNotificationsForType(type);
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Can be different from browser();
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;
};

#if BUILDFLAG(IS_CHROMEOS)
using WebAppNotificationsBrowserTest_IconAndTitleEnabled =
    WebAppNotificationsBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_IconAndTitleEnabled,
                       PersistentNotificationIconAndTitle) {
  const GURL app_url =
      https_server()->GetURL("/web_app_notifications/index.html");

  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  // The installation opens a new Browser window: |user_display_mode| is
  // kStandalone.
  SetAppBrowserForAppId(app_id);

  EXPECT_TRUE(RequestAndAcceptPermission());

  EXPECT_TRUE(AwaitScript("awaitServiceWorkerActivation()").ExtractBool());

  {
    EXPECT_TRUE(AwaitScript("displayPersistentNotification()").ExtractBool());

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(/*is_persistent=*/true);
    ASSERT_EQ(1u, notifications.size());

    const message_center::Notification& notification = notifications[0];

    EXPECT_EQ(u"Notification Title", notification.title());

    ASSERT_TRUE(notification.notifier_id().title.has_value());
    EXPECT_EQ(u"Web App Notifications Test",
              notification.notifier_id().title.value());

    ASSERT_FALSE(notification.small_image().IsEmpty());
    const SkBitmap monochrome_badge = *notification.small_image().ToSkBitmap();

    EXPECT_EQ(16, monochrome_badge.width());
    EXPECT_EQ(16, monochrome_badge.height());

    // the center of web_app_notifications/monochrome-32.png is transparent.
    EXPECT_EQ(
        color_utils::SkColorToRgbaString(SK_ColorTRANSPARENT),
        color_utils::SkColorToRgbaString(monochrome_badge.getColor(8, 8)));

    // theme_color in web_app_notifications/manifest.json is red.
    EXPECT_EQ(
        color_utils::SkColorToRgbaString(SK_ColorRED),
        color_utils::SkColorToRgbaString(monochrome_badge.getColor(0, 0)));

    EXPECT_TRUE(AwaitScript("closeAllPersistentNotifications()").ExtractBool());
  }

  {
    EXPECT_TRUE(
        AwaitScript("displayPersistentNotificationWithBadge()").ExtractBool());

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(/*is_persistent=*/true);
    ASSERT_EQ(1u, notifications.size());

    const message_center::Notification& notification = notifications[0];

    EXPECT_EQ(u"Notification With Badge", notification.title());

    ASSERT_TRUE(notification.notifier_id().title.has_value());
    EXPECT_EQ(u"Web App Notifications Test",
              notification.notifier_id().title.value());

    // small_image() here is chrome/test/data/web_app_notifications/blue-32.png.
    ASSERT_FALSE(notification.small_image().IsEmpty());
    const SkBitmap badge_from_js = *notification.small_image().ToSkBitmap();

    EXPECT_EQ(32, badge_from_js.width());
    EXPECT_EQ(32, badge_from_js.height());

    EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorBLUE),
              color_utils::SkColorToRgbaString(badge_from_js.getColor(8, 8)));

    EXPECT_TRUE(AwaitScript("closeAllPersistentNotifications()").ExtractBool());
  }
}
#else
using WebAppNotificationsBrowserTest_IconAndTitleDisabled =
    WebAppNotificationsBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_IconAndTitleDisabled,
                       PersistentNotificationIconAndTitle) {
  const GURL app_url =
      https_server()->GetURL("/web_app_notifications/index.html");

  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  // The installation opens a new Browser window: |user_display_mode| is
  // kStandalone.
  SetAppBrowserForAppId(app_id);

  EXPECT_TRUE(RequestAndAcceptPermission());

  EXPECT_TRUE(AwaitScript("awaitServiceWorkerActivation()").ExtractBool());

  {
    EXPECT_TRUE(AwaitScript("displayPersistentNotification()").ExtractBool());

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(/*is_persistent=*/true);
    ASSERT_EQ(1u, notifications.size());

    const message_center::Notification& notification = notifications[0];

    EXPECT_EQ(u"Notification Title", notification.title());

    ASSERT_FALSE(notification.notifier_id().title.has_value());
    EXPECT_TRUE(notification.small_image().IsEmpty());

    EXPECT_TRUE(AwaitScript("closeAllPersistentNotifications()").ExtractBool());
  }

  {
    EXPECT_TRUE(
        AwaitScript("displayPersistentNotificationWithBadge()").ExtractBool());

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(/*is_persistent=*/true);
    ASSERT_EQ(1u, notifications.size());

    const message_center::Notification& notification = notifications[0];

    EXPECT_EQ(u"Notification With Badge", notification.title());

    EXPECT_FALSE(notification.notifier_id().title.has_value());

    // small_image() here is chrome/test/data/web_app_notifications/blue-32.png.
    ASSERT_FALSE(notification.small_image().IsEmpty());
    const SkBitmap badge_from_js = *notification.small_image().ToSkBitmap();

    EXPECT_EQ(32, badge_from_js.width());
    EXPECT_EQ(32, badge_from_js.height());

    EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorBLUE),
              color_utils::SkColorToRgbaString(badge_from_js.getColor(8, 8)));

    EXPECT_TRUE(AwaitScript("closeAllPersistentNotifications()").ExtractBool());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
class WebAppNotificationsBrowserTest_MacPermissions
    : public WebAppNotificationsBrowserTest {
 public:
  WebAppNotificationsBrowserTest_MacPermissions()
      : WebAppNotificationsBrowserTest(
            {features::kAppShimNotificationAttribution},
            {}) {}

  void SetUpOnMainThread() override {
    WebAppNotificationsBrowserTest::SetUpOnMainThread();

    const GURL app_url =
        https_server()->GetURL("/web_app_notifications/index.html");

    const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
    // The installation opens a new Browser window: |user_display_mode| is
    // kStandalone.
    SetAppBrowserForAppId(app_id);
  }

  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->profile());
    WebAppNotificationsBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions, Granted) {
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionGranted));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       PreviouslyGranted_AcceptBubble) {
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyGranted,
                           permissions::PermissionRequestManager::ACCEPT_ALL));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       PreviouslyGranted_DenyBubble) {
  EXPECT_EQ("denied", RequestAndRespondToPermission(
                          mac_notifications::mojom::RequestPermissionResult::
                              kPermissionPreviouslyGranted,
                          permissions::PermissionRequestManager::DENY_ALL));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       RequestFailed_AcceptBubble) {
  EXPECT_EQ(
      "granted",
      RequestAndRespondToPermission(
          mac_notifications::mojom::RequestPermissionResult::kRequestFailed,
          permissions::PermissionRequestManager::ACCEPT_ALL));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       RequestFailed_DenyBubble) {
  EXPECT_EQ(
      "denied",
      RequestAndRespondToPermission(
          mac_notifications::mojom::RequestPermissionResult::kRequestFailed,
          permissions::PermissionRequestManager::DENY_ALL));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions, Denied) {
  EXPECT_EQ("denied", RequestAndRespondToPermission(
                          mac_notifications::mojom::RequestPermissionResult::
                              kPermissionDenied));
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       PreviouslyDenied) {
  EXPECT_EQ("default", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyDenied));
}

#endif

}  // namespace web_app
