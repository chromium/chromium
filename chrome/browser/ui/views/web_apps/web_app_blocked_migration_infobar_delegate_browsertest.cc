// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_blocked_migration_infobar_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_page_waiter.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

namespace web_app {

class WebAppBlockedMigrationInfoBarDelegateBrowserTest
    : public WebAppBrowserTestBase {
 public:
  WebAppBlockedMigrationInfoBarDelegateBrowserTest()
      : WebAppBrowserTestBase({blink::features::kWebAppMigrationApi}, {}) {}
  WebAppBlockedMigrationInfoBarDelegateBrowserTest(
      const WebAppBlockedMigrationInfoBarDelegateBrowserTest&) = delete;
  WebAppBlockedMigrationInfoBarDelegateBrowserTest& operator=(
      const WebAppBlockedMigrationInfoBarDelegateBrowserTest&) = delete;

  ~WebAppBlockedMigrationInfoBarDelegateBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    test_clock_.SetNow(base::Time::Now());
    provider().SetClockForTesting(&test_clock_);
  }

  void TearDownOnMainThread() override {
    provider().SetClockForTesting(base::DefaultClock::GetInstance());
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  base::SimpleTestClock* clock() { return &test_clock_; }

  void ClickInfoBarAccept(content::WebContents* web_contents) {
    infobars::InfoBar* infobar =
        WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents);
    ASSERT_TRUE(infobar);
    views::test::ButtonTestApi(
        static_cast<ConfirmInfoBar*>(infobar)->ok_button_for_testing())
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON,
                                    ui::EF_LEFT_MOUSE_BUTTON));
  }

  void ClickInfoBarClose(content::WebContents* web_contents) {
    infobars::InfoBar* infobar =
        WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents);
    ASSERT_TRUE(infobar);
    views::test::ButtonTestApi(
        static_cast<views::Button*>(
            static_cast<InfoBarView*>(infobar)->close_button()))
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON,
                                    ui::EF_LEFT_MOUSE_BUTTON));
  }

 private:
  base::SimpleTestClock test_clock_;
};

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       ShowInfoBarForPolicyAppWithPendingMigration) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       KeepInfoBarWhenWebAppNavigateAway) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));

  // Mock navigates to an out-of-scope URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, GURL("about:blank")));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       RemoveInfoBarWhenReparentBackToNonAppBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));

  // Reparent back to non-app browser
  BrowserWindowInterface* tabbed_browser = chrome::OpenInChrome(app_browser);
  content::WebContents* tabbed_web_contents =
      tabbed_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_EQ(web_contents, tabbed_web_contents);

  EXPECT_FALSE(
      WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

// Regression test for crbug.com/494294070. The infobar blocking migration shows
// up without a navigation to the destination app.
IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       InfoBarShowsUpWithoutNavigationToDestApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();
  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Wait for the destination app to also be installed.
  EXPECT_TRUE(test::WebAppPageWaiter(web_contents)
                  .ExpectUrl(app_url)
                  .ExpectManifest()
                  .WaitAndFlushCommands());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       RemoveInfoBarWhenMigrationIsGone) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  webapps::AppId target_app_id =
      ForceInstallWebApp(profile(), target_app_url).value();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(test::WebAppPageWaiter(web_contents)
                  .ExpectUrl(app_url)
                  .ExpectManifest()
                  .WaitAndFlushCommands());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));

  const GURL no_migration_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/no_migration.html");

  // Navigate the target app to a page that lacks the migrate_from field in its
  // manifest using target app browser. This should trigger a manifest update
  // that removes the pending_migration_info from the source app.
  Browser* target_app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), target_app_id);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(target_app_browser, no_migration_url));

  EXPECT_TRUE(test::WebAppPageWaiter(
                  target_app_browser->tab_strip_model()->GetActiveWebContents())
                  .ExpectUrl(no_migration_url)
                  .ExpectManifest()
                  .WaitAndFlushCommands());

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(
      WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       DoNotShowInfoBarIfDismissedRecently) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  ClickInfoBarClose(web_contents);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  app_browser = ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  web_contents = app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(
      WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       ShowInfoBarIfDismissedMoreThanAWeekAgo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  ClickInfoBarClose(web_contents);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  clock()->Advance(base::Days(8));

  app_browser = ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  web_contents = app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateBrowserTest,
                       InfobarDismissalUpdatesRegistry) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/suggest.html");
  webapps::AppId app_id = ForceInstallWebApp(profile(), app_url).value();

  const GURL target_app_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  ForceInstallWebApp(profile(), target_app_url);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  Browser* app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  ClickInfoBarClose(web_contents);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  ASSERT_TRUE(app->pending_migration_info().has_value());
  EXPECT_TRUE(app->pending_migration_info()->last_ignored_time().has_value());
}

class WebAppBlockedMigrationInfoBarDelegateUiTest
    : public SupportsTestUi<WebAppBrowserTestBase, TestBrowserUi> {
 public:
  WebAppBlockedMigrationInfoBarDelegateUiTest()
      : SupportsTestUi<WebAppBrowserTestBase, TestBrowserUi>(
            std::vector<base::test::FeatureRef>{
                blink::features::kWebAppMigrationApi},
            std::vector<base::test::FeatureRef>{}) {}

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL app_url = embedded_test_server()->GetURL(
        "/web_apps/migration/migrate_from/suggest.html");
    app_id_ = ForceInstallWebApp(profile(), app_url).value();

    const GURL target_app_url = embedded_test_server()->GetURL(
        "/web_apps/migration/migrate_to/suggest.html");
    ForceInstallWebApp(profile(), target_app_url);
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    app_browser_ = ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id_);

    views::test::WidgetVisibleWaiter waiter(
        BrowserView::GetBrowserViewForBrowser(app_browser_)->GetWidget());
    waiter.Wait();
  }

  bool VerifyUi() override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::View* infobar_container = browser_view->infobar_container();
    if (!infobar_container) {
      return false;
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(infobar_container, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose(app_browser_);
  }

  void TearDownOnMainThread() override {
    app_browser_ = nullptr;
    SupportsTestUi<WebAppBrowserTestBase,
                   TestBrowserUi>::TearDownOnMainThread();
  }

 private:
  webapps::AppId app_id_;
  raw_ptr<Browser> app_browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebAppBlockedMigrationInfoBarDelegateUiTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace web_app
