// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_window_open_permission_service.h"

#include <optional>
#include <string>
#include <vector>

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_window_open_permission_service_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

class Browser;

namespace web_app {
namespace {

constexpr std::string kIsolatedApp1DefaultName = "IWA 1";
constexpr std::string kIsolatedApp2DefaultName = "IWA 2";
constexpr std::string_view kIsolatedAppVersion = "1.0.0";

std::optional<IsolationData::OpenedTabsCounterNotificationState>
ReadIwaNotificationStateWithLock(const webapps::AppId& app_id,
                                 AppLock& lock,
                                 base::DictValue& debug_value) {
  const WebApp* web_app = lock.registrar().GetAppById(app_id);
  if (!web_app) {
    return std::nullopt;
  }
  return web_app->isolation_data()->opened_tabs_counter_notification_state();
}

}  // namespace

class IsolatedWebAppsWindowOpenPermissionServiceBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppsWindowOpenPermissionServiceBrowserTest() = default;

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();

    WebAppProvider* provider = WebAppProvider::GetForTest(browser()->profile());
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    isolated_web_apps_window_open_permission_service_ =
        IsolatedWebAppsWindowOpenPermissionServiceFactory::GetForProfile(
            profile());
  }

  void TearDownOnMainThread() override {
    isolated_web_apps_window_open_permission_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  webapps::AppId InstallIsolatedWebApp(
      std::string_view name = kIsolatedApp1DefaultName,
      const web_package::test::KeyPair& key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom()) {
    return IsolatedWebAppBuilder(
               ManifestBuilder().SetName(name).SetVersion(kIsolatedAppVersion))
        .BuildBundle(key_pair)
        ->InstallChecked(profile())
        .app_id();
  }

  webapps::AppId ForceInstallIsolatedWebApp(
      std::string_view name = kIsolatedApp1DefaultName,
      const web_package::test::KeyPair& key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom()) {
    return IsolatedWebAppBuilder(
               ManifestBuilder().SetName(name).SetVersion(kIsolatedAppVersion))
        .BuildBundle(key_pair)
        ->InstallWithSource(profile(),
                            &IsolatedWebAppInstallSource::FromExternalPolicy)
        ->app_id();
  }

  Browser* OpenIwaWindow(const webapps::AppId& app_id) {
    Browser* app_browser =
        ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    EXPECT_TRUE(app_browser);
    return app_browser;
  }

  content::WebContents* OpenChildWindowFromIwaBrowser(
      content::WebContents* opener_contents,
      const GURL& url,
      const std::string& target = "_blank",
      const std::string& features = "") {
    EXPECT_TRUE(opener_contents);

    content::WebContentsAddedObserver windowed_observer;
    EXPECT_TRUE(ExecJs(opener_contents->GetPrimaryMainFrame(),
                       "window.open('" + url.spec() + "', '" + target + "', '" +
                           features + "');"));

    content::WebContents* new_contents = windowed_observer.GetWebContents();
    WaitForLoadStopWithoutSuccessCheck(new_contents);

    return new_contents;
  }

  void WaitForPersistedState(const webapps::AppId& app_id,
                             int expected_times_shown) {
    base::test::TestFuture<
        std::optional<IsolationData::OpenedTabsCounterNotificationState>>
        future;
    WebAppProvider::GetForTest(profile())
        ->scheduler()
        .ScheduleCallbackWithResult(
            "ReadIwaNotificationState", AppLockDescription(app_id),
            base::BindOnce(&ReadIwaNotificationStateWithLock, app_id),
            future.GetCallback(),
            std::optional<IsolationData::OpenedTabsCounterNotificationState>(
                std::nullopt));
    ASSERT_TRUE(future.Wait());
    auto state = future.Take();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->times_shown(), expected_times_shown);
  }

  content::WebContents* OpenChildWindowAndExpectNotification(
      content::WebContents* opener_contents,
      const GURL& child_url,
      const webapps::AppId& app_id,
      base::test::TestFuture<void>& notification_added_future,
      std::string app_display_name = kIsolatedApp1DefaultName) {
    content::WebContents* child_contents =
        OpenChildWindowFromIwaBrowser(opener_contents, child_url);

    EXPECT_TRUE(notification_added_future.Wait());
    CheckNotificationContents(app_id, app_display_name);

    notification_added_future.Clear();
    return child_contents;
  }

  std::vector<message_center::Notification> GetDisplayedNotifications() {
    return display_service_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::TRANSIENT);
  }

  size_t GetNotificationCount() { return GetDisplayedNotifications().size(); }

  void CheckNotificationContents(
      const webapps::AppId& app_id,
      std::string app_display_name = kIsolatedApp1DefaultName) {
    std::optional<message_center::Notification> notification =
        display_service_tester_->GetNotification(
            GetNotificationIdForApp(app_id));
    ASSERT_TRUE(notification.has_value());

    std::u16string expected_title = base::UTF8ToUTF16(
        app_display_name + " automatically opens new windows.");
    EXPECT_EQ(notification->title(), expected_title);
    EXPECT_EQ(notification->message(),
              u"This allows the app to run as expected.");
    ASSERT_EQ(notification->buttons().size(), 1u);
    EXPECT_EQ(notification->buttons()[0].title, u"Manage permissions");
  }

 protected:
  std::string GetNotificationIdForApp(const webapps::AppId& app_id) {
    return "isolated_web_apps_window_open_permission_notification_" + app_id;
  }

  raw_ptr<IsolatedWebAppsWindowOpenPermissionService>
      isolated_web_apps_window_open_permission_service_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
    SingleIwaIsolatedWebAppsWindowOpenPermissionServiceNotification) {
  webapps::AppId app_id = InstallIsolatedWebApp();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowAndExpectNotification(
      iwa_opener_web_contents, GURL("https://example.com/app1/child1"), app_id,
      notification_added_future, kIsolatedApp1DefaultName);

  // Subsequent opens re-trigger the notification to reshow it to the user.
  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/app1/child2"));
  EXPECT_TRUE(notification_added_future.IsReady());
  notification_added_future.Clear();

  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/app1/child3"));

  EXPECT_EQ(1u, GetNotificationCount());
  EXPECT_TRUE(notification_added_future.IsReady());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       MultipleOpenerMultipleNotifiations) {
  webapps::AppId app1_id = InstallIsolatedWebApp();
  webapps::AppId app2_id = InstallIsolatedWebApp(kIsolatedApp2DefaultName);

  Browser* iwa1_browser = OpenIwaWindow(app1_id);
  Browser* iwa2_browser = OpenIwaWindow(app2_id);

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowAndExpectNotification(
      iwa1_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app1/child1"), app1_id,
      notification_added_future, kIsolatedApp1DefaultName);
  EXPECT_EQ(1u, GetNotificationCount());

  OpenChildWindowAndExpectNotification(
      iwa2_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app2/child1"), app2_id,
      notification_added_future, kIsolatedApp2DefaultName);
  EXPECT_EQ(2u, GetNotificationCount());

  CloseAndWait(iwa1_browser);
  CloseAndWait(iwa2_browser);

  EXPECT_EQ(2u, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       ShowNotificationPerIwaAtMostThreeTimes) {
  webapps::AppId app_id = InstallIsolatedWebApp();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  for (int i = 0; i < 3; i++) {
    base::test::TestFuture<void> notification_added_future;
    display_service_tester_->SetNotificationAddedClosure(
        notification_added_future.GetRepeatingCallback());
    OpenChildWindowFromIwaBrowser(
        iwa_opener_web_contents,
        GURL("https://example.com/s" + base::NumberToString(i) + "/child1"));

    ASSERT_TRUE(notification_added_future.Wait());
    EXPECT_EQ(1u, GetNotificationCount());
    std::optional<message_center::Notification> notification =
        display_service_tester_->GetNotification(
            GetNotificationIdForApp(app_id));
    ASSERT_TRUE(notification.has_value());
    base::test::TestFuture<void> notification_closed_future;
    display_service_tester_->SetNotificationClosedClosure(
        notification_closed_future.GetRepeatingCallback());

    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::TRANSIENT, notification->id(),
        /*by_user=*/false);

    ASSERT_TRUE(notification_closed_future.Wait());
    notification_closed_future.Clear();
    EXPECT_EQ(0u, GetNotificationCount());
    WaitForPersistedState(app_id, /*expected_times_shown=*/i + 1);
  }

  // Attempt to open a fourth window. The notification should NOT be shown.
  {
    base::test::TestFuture<void> notification_added_future_fail;
    display_service_tester_->SetNotificationAddedClosure(
        notification_added_future_fail.GetRepeatingCallback());
    OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                  GURL("https://example.com/s4/child1"));
    EXPECT_FALSE(notification_added_future_fail.IsReady());
    EXPECT_EQ(0u, GetNotificationCount());
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       PRE_TimesShownCounterPersistence) {
  webapps::AppId app_id = InstallIsolatedWebApp(
      "IWA1", web_package::test::GetDefaultEd25519KeyPair());
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/s1/child1"));
  ASSERT_TRUE(notification_added_future.Wait());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       TimesShownCounterPersistence) {
  webapps::AppId app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              web_package::test::GetDefaultEd25519WebBundleId())
                              .app_id();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();
  const std::string notification_id = GetNotificationIdForApp(app_id);

  auto check_persisted_state = [&](int expected_times_shown,
                                   bool expected_acknowledged) {
    base::test::TestFuture<
        std::optional<IsolationData::OpenedTabsCounterNotificationState>>
        future;
    WebAppProvider::GetForTest(profile())
        ->scheduler()
        .ScheduleCallbackWithResult(
            "ReadIwaNotificationState", AppLockDescription(app_id),
            base::BindOnce(&ReadIwaNotificationStateWithLock, app_id),
            future.GetCallback(),
            std::optional<IsolationData::OpenedTabsCounterNotificationState>(
                std::nullopt));

    EXPECT_TRUE(future.Wait());
    auto state = future.Take();

    EXPECT_TRUE(state.has_value());
    EXPECT_EQ(state->times_shown(), expected_times_shown);
    EXPECT_EQ(state->acknowledged(), expected_acknowledged);
  };

  check_persisted_state(/*expected_times_shown=*/1,
                        /*expected_acknowledged=*/false);

  {
    base::test::TestFuture<void> notification_added_future;
    display_service_tester_->SetNotificationAddedClosure(
        notification_added_future.GetRepeatingCallback());
    OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                  GURL("https://example.com/s2/child1"));
    ASSERT_TRUE(notification_added_future.Wait());

    std::optional<message_center::Notification> notification =
        display_service_tester_->GetNotification(notification_id);
    EXPECT_TRUE(notification.has_value());

    notification->delegate()->Close(/*by_user=*/true);
  }

  check_persisted_state(/*expected_times_shown=*/2,
                        /*expected_acknowledged=*/true);

  {
    OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                  GURL("https://example.com/s3/child1"));
    EXPECT_EQ(0u, GetNotificationCount());
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       PRE_AcknowledgedFieldPersistence) {
  webapps::AppId app_id = InstallIsolatedWebApp(
      "IWA1", web_package::test::GetDefaultEd25519KeyPair());
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();
  const std::string notification_id = GetNotificationIdForApp(app_id);

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/s1/child1"));
  ASSERT_TRUE(notification_added_future.Wait());
  std::optional<message_center::Notification> notification =
      display_service_tester_->GetNotification(notification_id);
  EXPECT_TRUE(notification.has_value());

  base::test::TestFuture<void> notification_closed_future;
  display_service_tester_->SetNotificationClosedClosure(
      notification_closed_future.GetRepeatingCallback());

  // User acknowledges the notification by closing the notification.
  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*by_user=*/true);
  ASSERT_TRUE(notification_closed_future.Wait());
  EXPECT_FALSE(display_service_tester_->GetNotification(notification_id));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       AcknowledgedFieldPersistence) {
  webapps::AppId app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              web_package::test::GetDefaultEd25519WebBundleId())
                              .app_id();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  auto check_persisted_state = [&](int expected_times_shown,
                                   bool expected_acknowledged) {
    base::test::TestFuture<
        std::optional<IsolationData::OpenedTabsCounterNotificationState>>
        future;
    WebAppProvider::GetForTest(profile())
        ->scheduler()
        .ScheduleCallbackWithResult(
            "ReadIwaNotificationState", AppLockDescription(app_id),
            base::BindOnce(&ReadIwaNotificationStateWithLock, app_id),
            future.GetCallback(),
            std::optional<IsolationData::OpenedTabsCounterNotificationState>(
                std::nullopt));

    ASSERT_TRUE(future.Wait());
    auto state = future.Get();

    EXPECT_EQ(state->times_shown(), expected_times_shown);
    EXPECT_EQ(state->acknowledged(), expected_acknowledged);
  };

  check_persisted_state(/*expected_times_shown=*/1,
                        /*expected_acknowledged=*/true);

  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/s2/child1"));

  // Because the notification has been acknowledged previously, it should not
  // be shown again.
  EXPECT_EQ(0u, GetNotificationCount());

  // Verify the persisted state remains unchanged, since no new notification
  // was shown.
  check_persisted_state(/*expected_times_shown=*/1,
                        /*expected_acknowledged=*/true);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       NoopenerArgumentDoesNotAffectCounters) {
  webapps::AppId app_id = InstallIsolatedWebApp();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowAndExpectNotification(
      iwa_opener_web_contents, GURL("https://example.com/app1/child1"), app_id,
      notification_added_future, kIsolatedApp1DefaultName);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       SelfTargetIsCountedAsOpenedByIwa) {
  webapps::AppId app_id = InstallIsolatedWebApp();

  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowAndExpectNotification(
      iwa_opener_web_contents, GURL("https://example.com/app1/child1"), app_id,
      notification_added_future, kIsolatedApp1DefaultName);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsWindowOpenPermissionServiceBrowserTest,
                       ForceInstalledIwaNeverShowsNotification) {
  webapps::AppId app_id = ForceInstallIsolatedWebApp();

  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                GURL("https://example.com/child1"));
  EXPECT_EQ(0u, GetNotificationCount());
}
}  // namespace web_app
