// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

#include <string>
#include <vector>

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_factory.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

class Browser;

namespace {

constexpr std::string kIsolatedApp1DefaultName = "IWA 1";
constexpr std::string kIsolatedApp2DefaultName = "IWA 2";
constexpr std::string_view kIsolatedAppVersion = "1.0.0";

}  // namespace

class IsolatedWebAppsOpenedTabsCounterServiceBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppsOpenedTabsCounterServiceBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    isolated_web_apps_opened_tabs_counter_service_ =
        IsolatedWebAppsOpenedTabsCounterServiceFactory::GetForProfile(
            profile());
  }

  void TearDownOnMainThread() override {
    isolated_web_apps_opened_tabs_counter_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }

  webapps::AppId InstallIsolatedWebApp(
      std::string_view name = kIsolatedApp1DefaultName) {
    auto app = web_app::IsolatedWebAppBuilder(
                   web_app::ManifestBuilder().SetName(name).SetVersion(
                       kIsolatedAppVersion))
                   .BuildBundle();
    return app->InstallChecked(profile()).app_id();
  }

  Browser* OpenIwaWindow(const webapps::AppId& app_id) {
    Browser* app_browser =
        web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
    EXPECT_TRUE(app_browser);
    return app_browser;
  }

  content::WebContents* OpenChildWindowFromIwaBrowser(
      content::WebContents* opener_contents,
      const GURL& url) {
    EXPECT_TRUE(opener_contents);

    content::WebContentsAddedObserver windowed_observer;
    EXPECT_TRUE(ExecJs(opener_contents->GetPrimaryMainFrame(),
                       "window.open('" + url.spec() + "');"));

    content::WebContents* new_contents = windowed_observer.GetWebContents();
    WaitForLoadStopWithoutSuccessCheck(new_contents);

    return new_contents;
  }

  content::WebContents* OpenChildWindowAndExpectNotificationContents(
      content::WebContents* opener_contents,
      const GURL& child_url,
      const webapps::AppId& app_id,
      int expected_window_count_in_notification,
      base::test::TestFuture<void>& notification_added_future,
      std::string app_display_name = kIsolatedApp1DefaultName) {
    content::WebContents* child_contents =
        OpenChildWindowFromIwaBrowser(opener_contents, child_url);

    EXPECT_TRUE(notification_added_future.Wait());
    CheckNotificationContents(app_id, expected_window_count_in_notification,
                              app_display_name);

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
      int opened_window_count,
      std::string app_display_name = kIsolatedApp1DefaultName) {
    std::optional<message_center::Notification> notification =
        display_service_tester_->GetNotification(
            GetNotificationIdForApp(app_id));
    ASSERT_TRUE(notification.has_value());

    std::u16string expected_title =
        base::UTF8ToUTF16(app_display_name + " has opened multiple windows.");
    EXPECT_EQ(notification->title(), expected_title);

    std::u16string expected_message =
        base::NumberToString16(opened_window_count) +
        u" new Chrome windows or tabs have been opened by this app. You "
        u"can manage this behavior under \"Pop-ups and Redirects\" permission.";
    EXPECT_EQ(notification->message(), expected_message);

    ASSERT_EQ(notification->buttons().size(), 1u);
    EXPECT_EQ(notification->buttons()[0].title, u"Change permissions");
  }

 protected:
  std::string GetNotificationIdForApp(const webapps::AppId& app_id) {
    return "isolated_web_apps_opened_tabs_counter_notification_" + app_id;
  }

  raw_ptr<IsolatedWebAppsOpenedTabsCounterService>
      isolated_web_apps_opened_tabs_counter_service_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
                       SingleIwaMultipleWebContentsOpenedServiceNotification) {
  webapps::AppId app_id = InstallIsolatedWebApp();
  content::WebContents* iwa_opener_web_contents =
      OpenIwaWindow(app_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  content::WebContents* iwa_child_browser1_contents =
      OpenChildWindowFromIwaBrowser(iwa_opener_web_contents,
                                    GURL("https://example.com/app1/child1"));
  ASSERT_FALSE(notification_added_future.IsReady());

  content::WebContents* iwa_child_browser2_contents =
      OpenChildWindowAndExpectNotificationContents(
          iwa_opener_web_contents, GURL("https://example.com/app1/child2"),
          app_id,
          /*expected_window_count_in_notification=*/2,
          notification_added_future, kIsolatedApp1DefaultName);

  OpenChildWindowAndExpectNotificationContents(
      iwa_opener_web_contents, GURL("https://example.com/app1/child3"), app_id,
      /*expected_window_count_in_notification=*/3, notification_added_future,
      kIsolatedApp1DefaultName);

  base::test::TestFuture<void> notification_closed_future;
  display_service_tester_->SetNotificationClosedClosure(
      notification_closed_future.GetRepeatingCallback());

  iwa_child_browser1_contents->Close();
  CheckNotificationContents(app_id, /*opened_window_count=*/2);
  iwa_child_browser2_contents->Close();
  ASSERT_TRUE(notification_closed_future.Wait());
  notification_closed_future.Clear();
  EXPECT_EQ(0u, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
                       MultipleOpenerMultipleNotifiations) {
  webapps::AppId app1_id = InstallIsolatedWebApp();
  webapps::AppId app2_id = InstallIsolatedWebApp(kIsolatedApp2DefaultName);

  Browser* iwa1_browser = OpenIwaWindow(app1_id);
  Browser* iwa2_browser = OpenIwaWindow(app2_id);

  // Child WebContents of the first app.
  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  OpenChildWindowFromIwaBrowser(
      iwa1_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app1/child1"));
  ASSERT_FALSE(notification_added_future.IsReady());

  OpenChildWindowAndExpectNotificationContents(
      iwa1_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app1/child2"), app1_id,
      /*expected_window_count_in_notification=*/2, notification_added_future,
      kIsolatedApp1DefaultName);

  // Child WebContents of the second app.
  OpenChildWindowFromIwaBrowser(
      iwa2_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app2/child1"));
  ASSERT_FALSE(notification_added_future.IsReady());

  OpenChildWindowAndExpectNotificationContents(
      iwa2_browser->tab_strip_model()->GetActiveWebContents(),
      GURL("https://example.com/app2/child2"), app2_id,
      /*expected_window_count_in_notification=*/2, notification_added_future,
      kIsolatedApp2DefaultName);
  EXPECT_EQ(2u, GetNotificationCount());

  web_app::CloseAndWait(iwa1_browser);
  web_app::CloseAndWait(iwa2_browser);

  // Notifications should remain even after parent window closures.
  EXPECT_EQ(2u, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
                       MoveOpenedTabToAnotherBrowserDoesNotAffectCounters) {
  webapps::AppId app1_id = InstallIsolatedWebApp(kIsolatedApp1DefaultName);
  content::WebContents* iwa1_opener_contents =
      OpenIwaWindow(app1_id)->tab_strip_model()->GetActiveWebContents();

  webapps::AppId app2_id = InstallIsolatedWebApp(kIsolatedApp2DefaultName);
  content::WebContents* iwa2_opener_contents =
      OpenIwaWindow(app2_id)->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<void> notification_added_future;
  display_service_tester_->SetNotificationAddedClosure(
      notification_added_future.GetRepeatingCallback());

  // Open two child windows from IWA 1.
  // This should trigger a notification for IWA 1.
  content::WebContents* app1_child1_contents = OpenChildWindowFromIwaBrowser(
      iwa1_opener_contents, GURL("https://example.com/app1/child1"));
  ASSERT_FALSE(notification_added_future.IsReady());

  OpenChildWindowAndExpectNotificationContents(
      iwa1_opener_contents, GURL("https://example.com/app1/child2"), app1_id,
      /*expected_window_count_in_notification=*/2, notification_added_future,
      kIsolatedApp1DefaultName);
  EXPECT_EQ(1u, GetNotificationCount());

  // Open two child windows from IWA 2.
  // This should trigger a notification for IWA 2.
  OpenChildWindowFromIwaBrowser(iwa2_opener_contents,
                                GURL("https://example.com/app2/child1"));
  ASSERT_FALSE(notification_added_future.IsReady());

  OpenChildWindowAndExpectNotificationContents(
      iwa2_opener_contents, GURL("https://example.com/app2/child2"), app2_id,
      /*expected_window_count_in_notification=*/2, notification_added_future,
      kIsolatedApp2DefaultName);
  EXPECT_EQ(2u, GetNotificationCount());

  ASSERT_TRUE(display_service_tester_->GetNotification(
      GetNotificationIdForApp(app1_id)));
  ASSERT_TRUE(display_service_tester_->GetNotification(
      GetNotificationIdForApp(app2_id)));

  Browser* another_browser = CreateBrowser(profile());

  // Move one of IWA 1's child windows (app1_child1_contents) to the
  // regular_browser.
  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(app1_child1_contents);
  Browser* original_app1_child1_browser =
      tab->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();

  int tab_index =
      original_app1_child1_browser->tab_strip_model()->GetIndexOfWebContents(
          app1_child1_contents);
  std::unique_ptr<content::WebContents> extracted_contents =
      original_app1_child1_browser->tab_strip_model()
          ->DetachWebContentsAtForInsertion(tab_index);

  another_browser->tab_strip_model()->AppendWebContents(
      std::move(extracted_contents), true);

  // Verify app1_child1_contents is now in another_browser.
  ASSERT_EQ(
      another_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
      GURL("https://example.com/app1/child1"));

  // No WebContents were destroyed, so no notification change events should
  // fire.
  EXPECT_EQ(2u, GetNotificationCount());
  CheckNotificationContents(app1_id, /*opened_window_count=*/2,
                            kIsolatedApp1DefaultName);
  CheckNotificationContents(app2_id, /*opened_window_count=*/2,
                            kIsolatedApp2DefaultName);
}
