// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/flat_map.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

base::flat_map</*error code*/ net::Error, /*error message*/ std::string>
    kTestCases{
        {net::ERR_INVALID_WEB_BUNDLE, "This application is missing or damaged"},
        {net::ERR_CONNECTION_REFUSED,
         "The development server for this application cannot be reached"},
        {net::ERR_INVALID_URL, "net::ERR_INVALID_URL"},
    };

}  // namespace

namespace web_app {

class IsolatedWebAppErrorPageTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  // Navigates IWA and fails with error
  Browser* LaunchIwaAndFailWithError(const webapps::AppId& app_id,
                                     const url::Origin& iwa_origin,
                                     net::Error error_code) {
    GURL starting_url = iwa_origin.GetURL().Resolve("/");

    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(starting_url,
                                                              error_code);
    return LaunchWebAppBrowserAndWait(app_id);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppErrorPageTest, UsesWebAppErrorPage) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));

  Browser* browser = LaunchIwaAndFailWithError(
      url_info.app_id(), url_info.origin(), net::ERR_INTERNET_DISCONNECTED);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Expect that the error page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  // "Test App" is the default value set by the ManifestBuilder
  EXPECT_EQ("Test App", EvalJs(web_contents, "document.title").ExtractString());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppErrorPageTest,
                       SetsCorrectMessageForErrorCode) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));

  for (auto& test_case : kTestCases) {
    net::Error error_code = test_case.first;
    const std::string& expected_message = test_case.second;
    SCOPED_TRACE(testing::Message()
                 << "error_code: " << error_code
                 << ", expected_message: " << expected_message);

    Browser* browser = LaunchIwaAndFailWithError(url_info.app_id(),
                                                 url_info.origin(), error_code);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();

    EXPECT_EQ(
        expected_message,
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg').textContent")
            .ExtractString());
  }
}

}  // namespace web_app
