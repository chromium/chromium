// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/device_form_factor.h"

namespace {
constexpr char kCrossOrigin[] = "cross.origin";
}

class SlimWebviewBrowserTest : public WebUIMochaBrowserTest {
 public:
  // While these tests are independent of Glic, we use the Glic host as a
  // convenient way to get a WebUI instance that has all the sources and
  // bindings set up correctly.
  SlimWebviewBrowserTest() { set_test_loader_host(chrome::kChromeUIGlicHost); }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpDefaultCommandLine(command_line);
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");

    // Capture headers for all requests to the test server and make them
    // available through a JSON endpoint.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &SlimWebviewBrowserTest::MonitorRequest, base::Unretained(this)));
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SlimWebviewBrowserTest::HandleRequest, base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    // Add a domain name that resolves to the local server. This is used for
    // cross-origin tests.
    host_resolver()->AddRule(kCrossOrigin, "127.0.0.1");
  }

 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    auto* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* permission_manager = PermissionManagerFactory::GetForProfile(profile);
    auto& contexts = permission_manager->PermissionContextsForTesting();
    auto it = contexts.find(content_settings::GeolocationContentSettingsType());
    bool can_get_location = true;
    if (it != contexts.end()) {
      auto* geolocation_context =
          static_cast<permissions::GeolocationPermissionContextAndroid*>(
              it->second.get());
      auto* settings = geolocation_context->GetLocationSettingsForTesting();
      can_get_location = settings->IsSystemLocationSettingEnabled() ||
                         settings->CanPromptToEnableSystemLocationSetting();
    }
    std::string script = content::JsReplace(
        "window.canGetLocation = $1; window.testServerUrl = $2; "
        "window.crossOriginUrl = $3;",
        can_get_location, embedded_test_server()->base_url().spec(),
        embedded_test_server()->GetURL(kCrossOrigin, "/").spec());
    ASSERT_TRUE(ExecJs(web_contents, script));
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    captured_headers_[request.relative_url] = request.headers;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() == "/capture-headers") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("application/json");
      // Allow all origins to make this request.
      http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");

      base::DictValue root;
      for (const auto& [url, headers] : captured_headers_) {
        base::DictValue headers_dict;
        for (const auto& [key, value] : headers) {
          headers_dict.Set(key, value);
        }
        root.Set(url, std::move(headers_dict));
      }

      std::string json;
      base::JSONWriter::Write(root, &json);
      http_response->set_content(json);
      return http_response;
    }
    return nullptr;
  }

  std::map<std::string, net::test_server::HttpRequest::HeaderMap>
      captured_headers_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kGlic};
};

IN_PROC_BROWSER_TEST_F(SlimWebviewBrowserTest, All) {
  RunTest("guest_view_shared/slim_webview_test.js", "mocha.run();");
}
