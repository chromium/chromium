// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

class WebAppInstallFromMigrateFromFieldCommandBrowserTest
    : public WebAppBrowserTestBase {
 public:
  WebAppInstallFromMigrateFromFieldCommandBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppMigrationApi,
         features::kWebAppPredictableAppUpdating},
        {});
  }

  void SetUp() override {
    https_server()->RegisterRequestHandler(base::BindRepeating(
        &WebAppInstallFromMigrateFromFieldCommandBrowserTest::
            RequestHandlerOverride,
        base::Unretained(this)));
    WebAppBrowserTestBase::SetUp();
  }

  // We need a request handler override because the manifest content depends on
  // the dynamically assigned port of the embedded test server. We cannot use
  // static files on disk because we don't know the port ahead of time.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() ==
        "/web_apps/standalone/target_manifest.json") {
      std::string manifest_template = R"(
        {
          "name": "Target App",
          "start_url": "$1",
          "display": "standalone",
          "id": "$2",
          "migrate_from": [
            {
              "id": "$3"
            }
          ]
        }
      )";
      std::string manifest_content = base::ReplaceStringPlaceholders(
          manifest_template,
          {GetTargetStartUrl().spec(), GetTargetManifestId().spec(),
           GetSourceManifestId().spec()},
          nullptr);

      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("application/json");
      http_response->set_content(manifest_content);
      return std::move(http_response);
    }
    return nullptr;
  }

  GURL GetSourceStartUrl() {
    return https_server()->GetURL("/banners/manifest_test_page.html");
  }
  webapps::ManifestId GetSourceManifestId() {
    return webapps::ManifestId(GetSourceStartUrl());
  }
  GURL GetTargetStartUrl() {
    return https_server()->GetURL("/banners/target_app.html");
  }
  webapps::ManifestId GetTargetManifestId() {
    return webapps::ManifestId(GetTargetStartUrl());
  }
  webapps::AppId GetTargetAppId() {
    return GenerateAppIdFromManifestId(GetTargetManifestId());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallFromMigrateFromFieldCommandBrowserTest,
                       SuccessfulMigration) {
  // 1. Install Source App.
  webapps::AppId source_app_id =
      InstallWebAppFromPage(browser(), GetSourceStartUrl());
  EXPECT_FALSE(source_app_id.empty());

  // 2. Navigate to a page that includes a manifest for the target app.
  GURL page_url = https_server()->GetURL(
      "/web_apps/standalone/basic.html?manifest=target_manifest.json");

  base::RunLoop run_loop;
  WebAppInstallManagerObserverAdapter observer(&provider().install_manager());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        if (installed_app_id == GetTargetAppId()) {
          run_loop.Quit();
        }
      }));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  run_loop.Run();

  // 3. Verify Target App is installed.
  const WebApp* target_app =
      provider().registrar_unsafe().GetAppById(GetTargetAppId());
  ASSERT_TRUE(target_app);
  EXPECT_EQ(target_app->untranslated_name(), "Target App");
  EXPECT_EQ(target_app->install_state(),
            proto::InstallState::SUGGESTED_FROM_MIGRATION);

  // 4. Wait for all commands to finish and verify source app's pending
  // migration.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* source_app =
      provider().registrar_unsafe().GetAppById(source_app_id);
  ASSERT_TRUE(source_app);
  ASSERT_TRUE(source_app->pending_migration_info().has_value());
  EXPECT_EQ(source_app->pending_migration_info()->manifest_id(),
            GetTargetManifestId().spec());
}

}  // namespace

}  // namespace web_app
