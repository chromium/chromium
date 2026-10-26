// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
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
    CHECK(display_service_tester_, base::NotFatalUntil::M162);
    return *display_service_tester_;
  }

  void SetAppBrowserForAppId(const webapps::AppId& app_id) {
    BrowserWindowInterface* app_browser = FindWebAppBrowser(profile(), app_id);
    ASSERT_TRUE(app_browser);
    app_browser_ = app_browser;
  }

  BrowserWindowInterface& app_browser() {
    CHECK(app_browser_, base::NotFatalUntil::M162);
    return *app_browser_;
  }

  content::WebContents* GetActiveWebContents() {
    return app_browser().tab_strip_model()->GetActiveWebContents();
  }

  content::EvalJsResult AwaitScript(const std::string& script) {
    return AwaitScript(GetActiveWebContents(), script);
  }

  content::EvalJsResult AwaitScript(content::WebContents* web_contents,
                                    const std::string& script) {
    content::EvalJsResult js_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(), script,
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
          permissions::PermissionRequestManager::ACCEPT_ALL) {
    return RequestAndRespondToPermissionIn(GetActiveWebContents(), os_response,
                                           bubble_response);
  }

  std::string RequestAndRespondToPermissionIn(
      content::WebContents* web_contents,
      mac_notifications::mojom::RequestPermissionResult os_response,
      permissions::PermissionRequestManager::AutoResponseType bubble_response =
          permissions::PermissionRequestManager::ACCEPT_ALL) {
    os_permission_request_count_ = 0;
    apps::AppShimManager::Get()
        ->SetNotificationPermissionRequestHandlerForTesting(
            base::BindLambdaForTesting(
                [this, os_response](
                    const webapps::AppId& app_id,
                    apps::AppShimManager::RequestNotificationPermissionCallback
                        callback) {
                  ++os_permission_request_count_;
                  std::move(callback).Run(os_response);
                }));
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(bubble_response);

    return AwaitScript(web_contents, "requestPermission()").ExtractString();
  }

  int OsPermissionRequestCount() const { return os_permission_request_count_; }
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
  raw_ptr<BrowserWindowInterface, AcrossTasksDanglingUntriaged> app_browser_ =
      nullptr;
#if BUILDFLAG(IS_MAC)
  int os_permission_request_count_ = 0;
#endif
};

#if BUILDFLAG(IS_CHROMEOS)
using WebAppNotificationsBrowserTest_IconAndTitleEnabled =
    WebAppNotificationsBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_IconAndTitleEnabled,
                       PersistentNotificationIconAndTitle) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/web_app_notifications/index.html");

  const webapps::AppId app_id = InstallWebAppFromPage(
      browser(), app_url, {.launch_or_reparent_page_to_app = true});
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
      embedded_https_test_server().GetURL("/web_app_notifications/index.html");

  const webapps::AppId app_id = InstallWebAppFromPage(
      browser(), app_url, {.launch_or_reparent_page_to_app = true});
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
            {features::kAppShimNotificationAttribution,
             features::kUseAdHocSigningForWebAppShims},
            {}) {}

  void SetUpOnMainThread() override {
    WebAppNotificationsBrowserTest::SetUpOnMainThread();

    app_url_ = embedded_https_test_server().GetURL(
        "/web_app_notifications/index.html");

    const webapps::AppId app_id = InstallWebAppFromPage(
        browser(), app_url_, {.launch_or_reparent_page_to_app = true});
    // The installation opens a new Browser window: |user_display_mode| is
    // kStandalone.
    SetAppBrowserForAppId(app_id);
    app_id_ = app_id;
  }

  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->GetProfile());
    WebAppNotificationsBrowserTest::TearDownOnMainThread();
  }

  const GURL& app_url() const { return app_url_; }
  const webapps::AppId& app_id() const { return app_id_; }

 protected:
  static constexpr char kOsPermissionRequestResultHistogram[] =
      "Notifications.Permissions.AppShim.OSPermissionRequestResult";

 private:
  GURL app_url_;
  webapps::AppId app_id_;
};

// Accepting the Chrome prompt also requests permission from macOS.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions, Granted) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionGranted));
  EXPECT_EQ(1, OsPermissionRequestCount());
  histogram_tester.ExpectUniqueSample(
      kOsPermissionRequestResultHistogram,
      mac_notifications::mojom::RequestPermissionResult::kPermissionGranted, 1);
}

// Accepting the Chrome prompt in a regular browser tab for a URL within the
// scope of an installed app also requests permission from macOS.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       GrantedInBrowserTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url()));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("granted",
            RequestAndRespondToPermissionIn(
                tab_contents, mac_notifications::mojom::
                                  RequestPermissionResult::kPermissionGranted));
  EXPECT_EQ(1, OsPermissionRequestCount());
}

// Accepting the Chrome prompt for a site that isn't an installed app doesn't
// request permission from macOS.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       NotInstalled) {
  test::UninstallWebApp(profile(), app_id());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url()));
  base::HistogramTester histogram_tester;
  EXPECT_EQ("granted", RequestAndRespondToPermissionIn(
                           browser()->tab_strip_model()->GetActiveWebContents(),
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionGranted));
  EXPECT_EQ(0, OsPermissionRequestCount());
  histogram_tester.ExpectTotalCount(kOsPermissionRequestResultHistogram, 0);
}

// Denying the Chrome prompt doesn't request permission from macOS.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       DenyBubble) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      "denied",
      RequestAndRespondToPermission(
          mac_notifications::mojom::RequestPermissionResult::kPermissionGranted,
          permissions::PermissionRequestManager::DENY_ALL));
  EXPECT_EQ(0, OsPermissionRequestCount());
  histogram_tester.ExpectTotalCount(kOsPermissionRequestResultHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       PreviouslyGranted) {
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyGranted));
  EXPECT_EQ(1, OsPermissionRequestCount());
}

// Even if the app shim is believed to already have macOS notification
// permission, a macOS permission request is still made, since the believed
// state could be stale (e.g. if permission was revoked in System Settings while
// the app shim wasn't running).
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       AlreadyGrantedInRegistry) {
  AppShimHost* host =
      apps::AppShimManager::Get()->FindHost(profile(), app_id());
  ASSERT_TRUE(host);
  host->SetNotificationPermissionStatusForTesting(
      mac_notifications::mojom::PermissionStatus::kGranted);
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyGranted));
  EXPECT_EQ(1, OsPermissionRequestCount());
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       RequestFailed) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      "granted",
      RequestAndRespondToPermission(
          mac_notifications::mojom::RequestPermissionResult::kRequestFailed));
  EXPECT_EQ(1, OsPermissionRequestCount());
  histogram_tester.ExpectUniqueSample(
      kOsPermissionRequestResultHistogram,
      mac_notifications::mojom::RequestPermissionResult::kRequestFailed, 1);
}

// If the page requests permission multiple times before the user responds to
// the Chrome prompt, accepting the prompt only results in a single macOS
// permission request.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       DuplicateRequestsAreCoalesced) {
  base::HistogramTester histogram_tester;
  std::vector<apps::AppShimManager::RequestNotificationPermissionCallback>
      os_callbacks;
  apps::AppShimManager::Get()
      ->SetNotificationPermissionRequestHandlerForTesting(
          base::BindLambdaForTesting(
              [&](const webapps::AppId& app_id,
                  apps::AppShimManager::RequestNotificationPermissionCallback
                      callback) {
                os_callbacks.push_back(std::move(callback));
              }));
  permissions::PermissionRequestManager::FromWebContents(GetActiveWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_EQ("granted,granted",
            AwaitScript("Promise.all([Notification.requestPermission(), "
                        "Notification.requestPermission()])"
                        ".then(results => results.join(','))"));
  ASSERT_EQ(1u, os_callbacks.size());
  std::move(os_callbacks[0])
      .Run(mac_notifications::mojom::RequestPermissionResult::
               kPermissionGranted);
  histogram_tester.ExpectUniqueSample(
      kOsPermissionRequestResultHistogram,
      mac_notifications::mojom::RequestPermissionResult::kPermissionGranted, 1);
}

// Clicking "Allow" grants the Chrome-level permission right away, regardless of
// the outcome of the subsequent macOS permission prompt.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions, Denied) {
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionDenied));
  EXPECT_EQ(1, OsPermissionRequestCount());
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       PreviouslyDenied) {
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyDenied));
  EXPECT_EQ(1, OsPermissionRequestCount());
}

// If macOS permission was explicitly denied, granting permission in Chrome
// results in the blocked-on-system-level indicator being shown.
IN_PROC_BROWSER_TEST_F(WebAppNotificationsBrowserTest_MacPermissions,
                       ExplicitlyDeniedShowsIndicator) {
  AppShimHost* host =
      apps::AppShimManager::Get()->FindHost(profile(), app_id());
  ASSERT_TRUE(host);
  // A denial following a grant (i.e. the user turning off notifications in
  // System Settings) is recorded as an explicit denial.
  host->SetNotificationPermissionStatusForTesting(
      mac_notifications::mojom::PermissionStatus::kGranted);
  host->SetNotificationPermissionStatusForTesting(
      mac_notifications::mojom::PermissionStatus::kDenied);
  ASSERT_EQ(
      AppShimRegistry::NotificationPermissionStatus::kExplicitlyDenied,
      AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(app_id()));

  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionPreviouslyDenied));
  EXPECT_EQ(1, OsPermissionRequestCount());

  auto image_model = ContentSettingImageModel::CreateForContentType(
      ContentSettingImageModel::ImageType::kNotifications);
  image_model->Update(GetActiveWebContents());
  EXPECT_TRUE(image_model->is_visible());
  EXPECT_TRUE(image_model->blocked_on_system_level());
}

class WebAppNotificationsBrowserTest_MacPermissionsAttributionDisabled
    : public WebAppNotificationsBrowserTest_MacPermissions {
 public:
  WebAppNotificationsBrowserTest_MacPermissionsAttributionDisabled() {
    feature_list_.InitAndDisableFeature(
        features::kAppShimNotificationAttribution);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Without notification attribution, accepting the Chrome prompt doesn't
// request permission from macOS.
IN_PROC_BROWSER_TEST_F(
    WebAppNotificationsBrowserTest_MacPermissionsAttributionDisabled,
    Granted) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ("granted", RequestAndRespondToPermission(
                           mac_notifications::mojom::RequestPermissionResult::
                               kPermissionGranted));
  EXPECT_EQ(0, OsPermissionRequestCount());
  histogram_tester.ExpectTotalCount(kOsPermissionRequestResultHistogram, 0);
}

#endif

}  // namespace web_app
