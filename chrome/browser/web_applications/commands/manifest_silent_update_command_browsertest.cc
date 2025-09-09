// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace web_app {

class ManifestSilentUpdateCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  ManifestSilentUpdateCommandBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kWebAppPredictableAppUpdating,
         features::kWebAppUsePrimaryIcon,
         blink::features::kWebAppEnableScopeExtensions},
        {});
  }
  ~ManifestSilentUpdateCommandBrowserTest() override = default;

  void SetUpOnMainThread() override {
    clock_ = std::make_unique<base::SimpleTestClock>();
    provider().SetClockForTesting(clock_.get());
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  std::unique_ptr<base::SimpleTestClock> clock_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest, SilentUpdate) {
  clock_->SetNow(base::Time::Now());
  const GURL app_url = https_server()->GetURL("/web_apps/updating/index.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Menu button should not have update available.
  WebAppMenuButton* const menu_button =
      static_cast<WebAppMenuButton*>(app_browser->GetBrowserView()
                                         .toolbar_button_provider()
                                         ->GetAppMenuButton());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());

  EXPECT_EQ(
      base::Time(),
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time());

  EXPECT_EQ(app_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/new_start_url_page.html");

  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_EQ(update_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // This was silent, so menu button should not have update available still
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest, PendingUpdate) {
  clock_->SetNow(base::Time::Now());
  const GURL app_url = https_server()->GetURL("/web_apps/updating/index.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Menu button should not have update available.
  WebAppMenuButton* const menu_button =
      static_cast<WebAppMenuButton*>(app_browser->GetBrowserView()
                                         .toolbar_button_provider()
                                         ->GetAppMenuButton());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());

  EXPECT_EQ(
      base::Time(),
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time());

  EXPECT_EQ(app_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/new_icon_page.html");

  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_EQ(update_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // This has a new icon, so a pending update should be here.
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());
  EXPECT_EQ(menu_button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_WEB_APP_MENU_BUTTON_TOOLTIP_UPDATE_AVAILABLE,
                app_browser->GetAppBrowserController()->GetAppShortName()));
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest,
                       ToolbarVisibilityUpdatedOnScopeChange) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/scope_updating/page.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  const GURL update_url =
      https_server()->GetURL("/web_apps/scope_updating/page_update.html");

  {
    UpdateAwaiter awaiter(
        WebAppProvider::GetForTest(browser()->profile())->install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
  }

  // After update, we are on the update page, which is in scope.
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Now navigate to the out-of-scope URL and check that the toolbar is
  // hidden because the scope has been widened.
  const GURL out_of_scope_url =
      https_server()->GetURL("/web_apps/scope_updating/out-of-scope.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, out_of_scope_url));
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

}  // namespace web_app
