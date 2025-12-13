// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "url/gurl.h"

using blink::mojom::ManifestLaunchHandler_ClientMode;

namespace web_app {

class WebAppNavigationCapturingIntentPickerBrowserTest
    : public WebAppNavigationCapturingBrowserTestBase {
 protected:
  GURL GetAppUrl() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index.html");
  }

  GURL GetAppUrlWithQuery() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html?q=fake_query_to_check_navigation");
  }

  GURL GetAppUrlWithWCO() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index_wco.html");
  }
};

// TODO(crbug.com/376641667): Flaky on Mac & Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_FocusExisting DISABLED_FocusExisting
#else
#define MAYBE_FocusExisting FocusExisting
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_FocusExisting) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateForTesting(
                     GetAppUrl(), blink::mojom::DisplayMode::kMinimalUi,
                     mojom::UserDisplayMode::kStandalone,
                     ManifestLaunchHandler_ClientMode::kFocusExisting));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_NE(app_browser, browser());
  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  WaitForLaunchParams(app_contents, /* min_launch_params_to_wait_for= */ 2);

  // Check the end state for the browser() -- it should have survived the Intent
  // Picker action.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Check the end state for the app.
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrl());

  std::vector<GURL> launch_params = apps::test::GetLaunchParamUrlsInContents(
      app_contents, "launchParamsTargetUrls");
  // There should be two launch params -- one for the initial launch and one
  // for when the existing app got focus (via the Intent Picker) and launch
  // params were enqueued.
  EXPECT_THAT(launch_params,
              testing::ElementsAre(GetAppUrl(), GetAppUrlWithQuery()));
}

// TODO(crbug.com/382315984): Fix this flake.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NavigateExisting DISABLED_NavigateExisting
#else
#define MAYBE_NavigateExisting NavigateExisting
#endif

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_NavigateExisting) {
  webapps::AppId app_id = InstallWebApp(WebAppInstallInfo::CreateForTesting(
      GetAppUrl(), blink::mojom::DisplayMode::kMinimalUi,
      mojom::UserDisplayMode::kStandalone,
      ManifestLaunchHandler_ClientMode::kNavigateExisting));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_NE(app_browser, browser());
  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  WaitForLaunchParams(app_contents,
                      /* min_launch_params_to_wait_for= */ 1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrlWithQuery());

  std::vector<GURL> launch_params = apps::test::GetLaunchParamUrlsInContents(
      app_contents, "launchParamsTargetUrls");
  // There should be one launch param -- because the Intent Picker triggers a
  // new navigation in the app (and launch params are then enqueued).
  EXPECT_THAT(launch_params, testing::ElementsAre(GetAppUrlWithQuery()));
}

// Test that the intent picker shows up for chrome://password-manager, since it
// is installable.
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       DoShowIconAndBubbleOnChromePasswordManagerPage) {
  GURL password_manager_url("chrome://password-manager");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), password_manager_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  webapps::AppId pwd_manager_app_id =
      web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(),
                                                       password_manager_url);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), password_manager_url);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForIntentPickerToShow(browser()));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       VerifyWindowControlsOverlayReappears) {
  auto ensure_app_browser =
      [&](base::FunctionRef<webapps::AppId()> app_browser_launcher) {
        ui_test_utils::BrowserCreatedObserver browser_created_observer;
        webapps::AppId app_id = app_browser_launcher();
        Browser* app_browser = browser_created_observer.Wait();
        EXPECT_NE(app_browser, browser());
        EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
        return std::make_pair(app_browser, app_id);
      };

  // Install WCO app and toggle the Window Controls Overlay display.
  std::pair<Browser*, webapps::AppId> install_data = ensure_app_browser(
      [&] { return InstallWebAppFromPage(browser(), GetAppUrlWithWCO()); });
  Browser* app_browser = install_data.first;
  const webapps::AppId app_id = install_data.second;

  // Toggle the Window Controls Overlay display in the current app_browser so
  // that the behavior is stored.
  base::test::TestFuture<void> test_future;
  content::WebContents* contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher1(contents, u"WCO Enabled");
  app_browser->GetBrowserView().ToggleWindowControlsOverlayEnabled(
      test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  std::ignore = title_watcher1.WaitAndGetTitle();
  ASSERT_TRUE(app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());

  // Disable navigation capturing for the app_id so that the enable link
  // capturing infobar shows up.
  ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_id),
            base::ok());

  // Navigate to the WCO app site, verify the intent picker icon shows up.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrlWithWCO()));
  ASSERT_TRUE(web_app::WaitForIntentPickerToShow(browser()));
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, nullptr);

  // Click on the intent picker icon, and verify an app browser gets launched
  // with no WCO. The link capturing infobar is shown.
  // `include_nestable_tasks` is set to true because the "default" RunLoop
  // hangs, waiting for certain nested tasks to finish.
  content::TitleWatcher title_watcher2(new_contents, u"WCO Disabled",
                                       /*include_nestable_tasks=*/true);
  std::pair<Browser*, webapps::AppId> post_intent_picker_data =
      ensure_app_browser([&] {
        EXPECT_TRUE(web_app::ClickIntentPickerChip(browser()));
        return app_id;
      });
  Browser* new_app_browser = post_intent_picker_data.first;
  std::ignore = title_watcher2.WaitAndGetTitle();
  EXPECT_FALSE(
      new_app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(
      apps::EnableLinkCapturingInfoBarDelegate::FindInfoBar(new_contents));

  // Close the infobar, and wait for the WCO to come back on.
  // `include_nestable_tasks` is set to true because the "default" RunLoop
  // hangs, waiting for certain nested tasks to finish.
  content::TitleWatcher title_watcher3(new_contents, u"WCO Enabled",
                                       /*include_nestable_tasks=*/true);
  apps::EnableLinkCapturingInfoBarDelegate::RemoveInfoBar(new_contents);
  std::ignore = title_watcher3.WaitAndGetTitle();
  EXPECT_TRUE(
      new_app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());
  EXPECT_FALSE(
      apps::EnableLinkCapturingInfoBarDelegate::FindInfoBar(new_contents));
}

}  // namespace web_app
