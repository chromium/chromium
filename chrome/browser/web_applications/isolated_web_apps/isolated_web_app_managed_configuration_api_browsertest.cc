// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace web_app {

using ::testing::Eq;

constexpr char kConfigurationUrl[] = "/conf1.json";
constexpr char kConfigurationHash[] = "asdas9jasidjd";
constexpr char kConfigurationData[] = R"(
{
  "key1": "value1",
  "key2" : 2
}
)";
constexpr char kKey1[] = "key1";
constexpr char kKey2[] = "key2";

struct ResponseTemplate {
  std::string response_body;
};

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    std::map<std::string, ResponseTemplate> templates,
    const net::test_server::HttpRequest& request) {
  if (!base::Contains(templates, request.relative_url)) {
    return std::make_unique<net::test_server::HungResponse>();
  }

  auto response_template = templates[request.relative_url];
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response;
  http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_template.response_body);
  http_response->set_content_type("text/plain");
  return http_response;
}

// Test the API behavior in Isolated Web App
class ManagedConfigurationAPIInIsolatedWebAppTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  web_package::SignedWebBundleId GetWebBundleId() const {
    return test::GetDefaultEd25519WebBundleId();
  }

  void EnableTestServer(
      const std::map<std::string, ResponseTemplate>& templates) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, templates));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetConfiguration(const std::string& conf_url,
                        const std::string& conf_hash,
                        const std::string& origin_key) {
    browser()->profile()->GetPrefs()->SetList(
        prefs::kManagedConfigurationPerOrigin,
        base::Value::List().Append(
            base::Value::Dict()
                .Set(ManagedConfigurationAPI::kOriginKey, origin_key)
                .Set(ManagedConfigurationAPI::kManagedConfigurationUrlKey,
                     embedded_test_server()->GetURL(conf_url).spec())
                .Set(ManagedConfigurationAPI::kManagedConfigurationHashKey,
                     conf_hash)));
  }

 protected:
  IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPIInIsolatedWebAppTest,
                       DataIsReachable) {
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(GetWebBundleId());

  EnableTestServer({{kConfigurationUrl, {kConfigurationData}}});
  SetConfiguration(kConfigurationUrl, kConfigurationHash,
                   url_info.origin().Serialize());

  update_server_mixin_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder())
          .BuildBundle(GetWebBundleId(), {test::GetDefaultEd25519KeyPair()}));

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListeningAndWait({url_info.app_id()});

  auto result = content::EvalJs(
      OpenApp(url_info.app_id(), ""),
      content::JsReplace("navigator.managed.getManagedConfiguration($1)",
                         base::Value::List().Append(kKey1).Append(kKey2)));

  EXPECT_EQ(result.value.GetDict(),
            *base::JSONReader::Read(kConfigurationData));
}

}  // namespace web_app
