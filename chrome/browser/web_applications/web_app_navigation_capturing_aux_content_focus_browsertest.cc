// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "url/gurl.h"

using blink::mojom::ManifestLaunchHandler_ClientMode;

namespace web_app {

class WebAppNavigationCapturingAuxContentFocusBrowserTest
    : public WebAppNavigationCapturingBrowserTestBase,
      public testing::WithParamInterface<ManifestLaunchHandler_ClientMode> {
 protected:
  GURL GetAppUrl() {
    return https_server()->GetURL("/web_apps/aux_content_no_focus/index.html");
  }
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingAuxContentFocusBrowserTest,
                       NoFocusForAuxContent) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateForTesting(
                     GetAppUrl(), blink::mojom::DisplayMode::kMinimalUi,
                     mojom::UserDisplayMode::kStandalone, GetParam()));

  // Launch the app in an auxiliary browsing context (opener = true).
  content::WebContents* intermediary_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(intermediary_tab);
  content::WebContents* aux_contents = CallWindowOpenExpectNewTab(
      intermediary_tab, GetAppUrl(), /* with_opener= */ true);
  ASSERT_TRUE(aux_contents);
  EXPECT_TRUE(aux_contents->HasOpener());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Putting the focus back on the initial tab is not necessary for the test
  // to function. But it helps with debugging, because you can visually
  // inspect that the aux_content tab isn't brought into focus again.
  intermediary_tab->Focus();

  Browser* app_browser = CallWindowOpenExpectNewBrowser(
      intermediary_tab, GetAppUrl(), /* with_opener= */ false);
  ASSERT_TRUE(app_browser);
  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(Browser::Type::TYPE_APP, app_browser->type());

  WaitForLaunchParams(app_contents, /* min_launch_params_to_wait_for= */ 1);
  std::vector<GURL> launch_params =
      GetLaunchParams(app_contents, "launchParamsTargetUrls");
  EXPECT_THAT(launch_params, testing::ElementsAre(GetAppUrl()));

  std::vector<GURL> aux_launch_params =
      GetLaunchParams(aux_contents, "launchParamsTargetUrls");
  EXPECT_EQ(0u, aux_launch_params.size());
}

INSTANTIATE_TEST_SUITE_P(
    ClientMode,
    WebAppNavigationCapturingAuxContentFocusBrowserTest,
    testing::Values(ManifestLaunchHandler_ClientMode::kFocusExisting,
                    ManifestLaunchHandler_ClientMode::kNavigateExisting));

}  // namespace web_app
