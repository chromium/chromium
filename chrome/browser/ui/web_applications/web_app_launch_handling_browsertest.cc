// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace web_app {

using RouteTo = LaunchHandler::RouteTo;
using NavigateExistingClient = LaunchHandler::NavigateExistingClient;

class WebAppLaunchHanderBrowserTest : public InProcessBrowserTest {
 public:
  WebAppLaunchHanderBrowserTest() = default;
  ~WebAppLaunchHanderBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  const WebApp* GetWebApp(const AppId& app_id) {
    return WebAppProvider::GetForTest(profile())->registrar().GetAppById(
        app_id);
  }

  absl::optional<LaunchHandler> GetLaunchHandler(const AppId& app_id) {
    return GetWebApp(app_id)->launch_handler();
  }

  std::string AwaitNextLaunchParamsTargetUrl(Browser* browser) {
    const char* script = R"(
      new Promise(resolve => {
        window.launchQueue.setConsumer(resolve);
      }).then(params => params.targetURL);
    )";
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kWebAppEnableLaunchHandler};
  ScopedOsHooksSuppress os_hooks_suppress_{
      OsIntegrationManager::ScopedSuppressOsHooksForTesting()};
};

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderBrowserTest, RouteToEmpty) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));
  EXPECT_EQ(GetLaunchHandler(app_id), absl::nullopt);

  Browser* browser_1 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderBrowserTest, RouteToAuto) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL(
                     "/web_apps/get_manifest.html?route_to_auto.json"));
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{RouteTo::kAuto, NavigateExistingClient::kAlways}));

  std::string start_url = GetWebApp(app_id)->start_url().spec();

  Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url);

  Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url);

  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderBrowserTest, RouteToNewClient) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL(
                     "/web_apps/get_manifest.html?route_to_new_client.json"));
  EXPECT_EQ(
      GetLaunchHandler(app_id),
      (LaunchHandler{RouteTo::kNewClient, NavigateExistingClient::kAlways}));

  std::string start_url = GetWebApp(app_id)->start_url().spec();

  Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url);

  Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url);

  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderBrowserTest, RouteToExistingClient) {
  AppId app_id = InstallWebAppFromPage(
      browser(),
      embedded_test_server()->GetURL(
          "/web_apps/"
          "get_manifest.html?route_to_existing_client_navigate_empty.json"));
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{RouteTo::kExistingClient,
                           NavigateExistingClient::kAlways}));

  Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?route_to=existing-client&navigate=empty");
  EXPECT_EQ(web_contents->GetVisibleURL(), start_url);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url.spec());

  // Navigate window away from start_url to check that the next launch navs to
  // start_url again.
  NavigateParams navigate_params(browser_1, GURL("about:blank"),
                                 ui::PAGE_TRANSITION_LINK);
  ASSERT_TRUE(Navigate(&navigate_params));
  EXPECT_EQ(web_contents->GetVisibleURL(), GURL("about:blank"));

  Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(web_contents->GetVisibleURL(), start_url);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url.spec());

  EXPECT_EQ(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderBrowserTest, GlobalLaunchQueue) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.launchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchParams").ExtractBool());
}

class WebAppLaunchHanderDisabledBrowserTest : public InProcessBrowserTest {
 public:
  WebAppLaunchHanderDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kWebAppEnableLaunchHandler);
  }
  ~WebAppLaunchHanderDisabledBrowserTest() override = default;

  Profile* profile() { return browser()->profile(); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedOsHooksSuppress os_hooks_suppress_{
      OsIntegrationManager::ScopedSuppressOsHooksForTesting()};
};

IN_PROC_BROWSER_TEST_F(WebAppLaunchHanderDisabledBrowserTest, NoLaunchQueue) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(EvalJs(web_contents, "!!window.LaunchQueue").ExtractBool());
  EXPECT_FALSE(EvalJs(web_contents, "!!window.launchQueue").ExtractBool());
  EXPECT_FALSE(EvalJs(web_contents, "!!window.LaunchParams").ExtractBool());
}

}  // namespace web_app
