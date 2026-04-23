// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

class GetInstalledRelatedAppsBrowserTest : public WebAppBrowserTestBase {
 public:
  GetInstalledRelatedAppsBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kWebAppEnableScopeExtensionsBySite}, {});
  }

  void SetUpOnMainThread() override {
    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&GetInstalledRelatedAppsBrowserTest::RequestHandler,
                            base::Unretained(this)));
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    std::string path(request.GetURL().path());
    auto it = url_overrides_.find(path);
    if (it == url_overrides_.end()) {
      return nullptr;
    }
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(it->second);
    return http_response;
  }

 protected:
  // Map from path to content.
  std::map<std::string, std::string> url_overrides_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GetInstalledRelatedAppsBrowserTest, SameOriginSuccess) {
  GURL app_url = embedded_https_test_server().GetURL("example.com",
                                                     "/web_apps/basic.html");
  webapps::ManifestId app_manifest_id = webapps::ManifestId(app_url.spec());

  // Install an app that lists itself as related.
  // We use $REQUEST_HOST/... to dynamically match the manifest id.
  std::string manifest =
      base::ReplaceStringPlaceholders(R"({
        "name": "Test App",
        "start_url": "/web_apps/basic.html",
        "scope": "/web_apps/",
        "related_applications": [
          {
            "platform": "webapp",
            "id": "$1"
          }
        ]
      })",
                                      {app_manifest_id.spec()}, nullptr);

  // basic.html in chrome/test/data/web_apps/ links to basic.json.
  // We override basic.json to include our dynamic ID.
  url_overrides_["/web_apps/basic.json"] = manifest;

  InstallWebAppInNewTabAndClose(browser(), app_url);

  // Navigate to the app page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       "navigator.getInstalledRelatedApps()");

  ASSERT_TRUE(result.is_ok());
  EXPECT_THAT(result.ExtractList(),
              testing::ElementsAre(base::test::DictionaryHasValue(
                  "id", base::Value(app_manifest_id.spec()))));
}

IN_PROC_BROWSER_TEST_F(GetInstalledRelatedAppsBrowserTest,
                       CrossOriginWithScopeExtensions_CurrentlyFails) {
  GURL app_url = embedded_https_test_server().GetURL("example.com",
                                                     "/web_apps/basic.html");
  webapps::ManifestId app_manifest_id = webapps::ManifestId(app_url.spec());

  GURL other_url =
      embedded_https_test_server().GetURL("foo.com", "/web_apps/basic.html");

  // Install an app on example.com that lists itself as related and has a scope
  // extension for foo.com.
  std::string manifest = base::ReplaceStringPlaceholders(
      R"({
        "name": "Test App",
        "start_url": "/web_apps/basic.html",
        "scope": "/web_apps/",
        "related_applications": [
          {
            "platform": "webapp",
            "id": "$1"
          }
        ],
        "scope_extensions": [
          { "type": "origin", "origin": "$2" }
        ]
      })",
      {app_manifest_id.spec(), other_url.GetWithEmptyPath().spec()}, nullptr);

  // Intercept the association file on example.com for foo.com.
  url_overrides_["/.well-known/web-app-origin-association"] =
      base::ReplaceStringPlaceholders(
          R"({
        "$1": { "scope": "/" }
      })",
          {app_manifest_id.spec()}, nullptr);

  url_overrides_["/web_apps/basic.json"] = manifest;
  webapps::AppId app_id = InstallWebAppInNewTabAndClose(browser(), app_url);

  EXPECT_TRUE(
      provider().registrar_unsafe().IsUrlInAppExtendedScope(other_url, app_id));

  // Navigate to foo.com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));

  auto result = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                       "navigator.getInstalledRelatedApps()");

  ASSERT_TRUE(result.is_ok());
  // TODO(crbug.com/491214102): If it is OK for this to be queried from an
  // extended origin, then this should return the app's manifest id.
  EXPECT_THAT(result.ExtractList(), testing::IsEmpty());
}

}  // namespace web_app
