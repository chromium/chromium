// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "url/gurl.h"

namespace web_app {

class WebAppNavigationCapturingIntentPickerBrowserTest
    : public WebAppBrowserTestBase {
 protected:
  WebAppNavigationCapturingIntentPickerBrowserTest() {
    std::map<std::string, std::string> parameters;
    parameters["link_capturing_state"] = "reimpl_default_on";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, parameters);
  }

  GURL GetAppUrl() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index.html");
  }

  GURL GetAppUrlWithQuery() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html?q=fake_query_to_check_navigation");
  }

  std::pair<Browser*, content::WebContents*> SetupAndLaunchApp(
      const GURL& url,
      blink::mojom::ManifestLaunchHandler_ClientMode client_mode) {
    webapps::AppId app_id = InstallApp(url, client_mode);

    Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_NE(app_browser, browser());
    content::WebContents* app_contents =
        app_browser->tab_strip_model()->GetWebContentsAt(0);
    return std::make_pair(app_browser, app_contents);
  }

  webapps::AppId InstallApp(
      const GURL& url,
      blink::mojom::ManifestLaunchHandler_ClientMode client_mode) {
    auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    web_app_info->launch_handler = blink::Manifest::LaunchHandler(client_mode);
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    const webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  void WaitForLoadStopAndLaunchParams(content::WebContents* app_contents,
                                      content::WebContents* tab_contents,
                                      int min_launch_params_to_wait_for) {
    content::WaitForLoadStop(app_contents);
    content::WaitForLoadStop(tab_contents);

    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(base::test::RunUntil([&] {
      return content::EvalJs(
                 app_contents,
                 "launchParamsTargetUrls.length >= " +
                     base::NumberToString(min_launch_params_to_wait_for))
          .ExtractBool();
    }));
  }

  std::vector<GURL> GetLaunchParams(content::WebContents* contents) {
    std::vector<GURL> launch_params;
    content::EvalJsResult launchParamsResults = content::EvalJs(
        contents->GetPrimaryMainFrame(),
        "'launchParamsTargetUrls' in window ? launchParamsTargetUrls : []");
    EXPECT_THAT(launchParamsResults, content::EvalJsResult::IsOk());
    base::Value::List launchParamsTargetUrls =
        launchParamsResults.ExtractList();
    if (!launchParamsTargetUrls.empty()) {
      for (const base::Value& url : launchParamsTargetUrls) {
        launch_params.push_back(GURL(url.GetString()));
      }
    }
    return launch_params;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/366547977): CrOS doesn't use our nav capturing implementation.
// TODO(crbug.com/376641667): Flaky on Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_FocusExisting DISABLED_FocusExisting
#else
#define MAYBE_FocusExisting FocusExisting
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_FocusExisting) {
  std::pair<Browser*, content::WebContents*> pair = SetupAndLaunchApp(
      GetAppUrl(),
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);
  auto [app_browser, app_contents] = pair;

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  // Grab a fresh tab_contents pointer to wait on.
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  WaitForLoadStopAndLaunchParams(app_contents, tab_contents,
                                 /* min_launch_params_to_wait_for= */ 2);

  // Check the end state for the browser() -- it should have survived the Intent
  // Picker action.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Check the end state for the app.
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrl());

  std::vector<GURL> launch_params = GetLaunchParams(app_contents);
  // There should be two launch params -- one for the initial launch and one
  // for when the existing app got focus (via the Intent Picker) and launch
  // params were enqueued.
  EXPECT_THAT(launch_params,
              testing::ElementsAre(GetAppUrl(), GetAppUrlWithQuery()));
}

// TODO(crbug.com/366547977): CrOS doesn't use our nav capturing implementation.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NavigateExisting DISABLED_NavigateExisting
#else
#define MAYBE_NavigateExisting NavigateExisting
#endif  // BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_NavigateExisting) {
  std::pair<Browser*, content::WebContents*> pair = SetupAndLaunchApp(
      GetAppUrl(),
      blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting);
  auto [app_browser, app_contents] = pair;

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  // Grab a fresh tab_contents pointer to wait on.
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  WaitForLoadStopAndLaunchParams(app_contents, tab_contents,
                                 /* min_launch_params_to_wait_for= */ 1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrlWithQuery());

  std::vector<GURL> launch_params = GetLaunchParams(app_contents);
  // There should be one launch param -- because the Intent Picker triggers a
  // new navigation in the app (and launch params are then enqueued).
  EXPECT_THAT(launch_params, testing::ElementsAre(GetAppUrlWithQuery()));
}

}  // namespace web_app
