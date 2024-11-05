// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
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
};

// TODO(crbug.com/376641667): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
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

  std::vector<GURL> launch_params =
      GetLaunchParams(app_contents, "launchParamsTargetUrls");
  // There should be two launch params -- one for the initial launch and one
  // for when the existing app got focus (via the Intent Picker) and launch
  // params were enqueued.
  EXPECT_THAT(launch_params,
              testing::ElementsAre(GetAppUrl(), GetAppUrlWithQuery()));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       NavigateExisting) {
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

  std::vector<GURL> launch_params =
      GetLaunchParams(app_contents, "launchParamsTargetUrls");
  // There should be one launch param -- because the Intent Picker triggers a
  // new navigation in the app (and launch params are then enqueued).
  EXPECT_THAT(launch_params, testing::ElementsAre(GetAppUrlWithQuery()));
}

}  // namespace web_app
