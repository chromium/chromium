// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_blocked_migration_infobar_delegate.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
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

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_manager);

  bool found_infobar = false;
  for (const auto& infobar : infobar_manager->infobars()) {
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE) {
      found_infobar = true;
      break;
    }
  }
  EXPECT_TRUE(found_infobar);
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
