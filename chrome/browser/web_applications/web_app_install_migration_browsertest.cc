// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

class WebAppInstallMigrationBrowserTest : public WebAppBrowserTestBase {
 public:
  void SetUp() override {
    RegisterPortReplacementHandler();
    RegisterAssociatedOriginWellKnownHandler(
        "www.example.com",
        "https://foo.example.com:$PORT/web_apps/migration/"
        "target_from_source/manifest_id");
    RegisterAssociatedOriginWellKnownHandler(
        "www.example.com",
        "https://www.example.org:$PORT/web_apps/migration/"
        "target_from_source/manifest_id");
    WebAppBrowserTestBase::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppMigrationApi};
};

IN_PROC_BROWSER_TEST_F(WebAppInstallMigrationBrowserTest,
                       SameOriginMigration_TriggersPendingMigration) {
  GURL source_url = embedded_https_test_server().GetURL(
      "www.example.com",
      "/web_apps/migration/source_to_www_example_com/index.html");
  webapps::AppId source_app_id = InstallWebAppFromPage(browser(), source_url);
  EXPECT_FALSE(source_app_id.empty());

  GURL target_url = embedded_https_test_server().GetURL(
      "www.example.com", "/web_apps/migration/target_from_source/index.html");
  webapps::AppId target_app_id = InstallWebAppFromPage(browser(), target_url);
  EXPECT_FALSE(target_app_id.empty());
  EXPECT_NE(source_app_id, target_app_id);

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInChrome()));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      target_app_id, WebAppFilter::InstalledInChrome()));

  const WebApp* source_app =
      provider().registrar_unsafe().GetAppById(source_app_id);
  const WebApp* target_app =
      provider().registrar_unsafe().GetAppById(target_app_id);
  ASSERT_TRUE(source_app->pending_migration_info().has_value());
  EXPECT_EQ(source_app->pending_migration_info()->manifest_id(),
            target_app->manifest_id());
}

IN_PROC_BROWSER_TEST_F(WebAppInstallMigrationBrowserTest,
                       CrossOriginSameSiteMigration_TriggersPendingMigration) {
  GURL source_url = embedded_https_test_server().GetURL(
      "www.example.com",
      "/web_apps/migration/source_to_foo_example_com/index.html");
  webapps::AppId source_app_id = InstallWebAppFromPage(browser(), source_url);
  EXPECT_FALSE(source_app_id.empty());

  GURL target_url = embedded_https_test_server().GetURL(
      "foo.example.com", "/web_apps/migration/target_from_source/index.html");
  webapps::AppId target_app_id = InstallWebAppFromPage(browser(), target_url);
  EXPECT_FALSE(target_app_id.empty());
  EXPECT_NE(source_app_id, target_app_id);

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInChrome()));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      target_app_id, WebAppFilter::InstalledInChrome()));

  const WebApp* source_app =
      provider().registrar_unsafe().GetAppById(source_app_id);
  const WebApp* target_app =
      provider().registrar_unsafe().GetAppById(target_app_id);
  ASSERT_TRUE(source_app->pending_migration_info().has_value());
  EXPECT_EQ(source_app->pending_migration_info()->manifest_id(),
            target_app->manifest_id());
}

IN_PROC_BROWSER_TEST_F(WebAppInstallMigrationBrowserTest,
                       CrossSiteMigration_DoesNotTriggerPendingMigration) {
  GURL source_url = embedded_https_test_server().GetURL(
      "www.example.com",
      "/web_apps/migration/source_to_www_example_org/index.html");
  webapps::AppId source_app_id = InstallWebAppFromPage(browser(), source_url);
  EXPECT_FALSE(source_app_id.empty());

  GURL target_url = embedded_https_test_server().GetURL(
      "www.example.org", "/web_apps/migration/target_from_source/index.html");
  webapps::AppId target_app_id = InstallWebAppFromPage(browser(), target_url);
  EXPECT_FALSE(target_app_id.empty());
  EXPECT_NE(source_app_id, target_app_id);

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInChrome()));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      target_app_id, WebAppFilter::InstalledInChrome()));

  const WebApp* source_app =
      provider().registrar_unsafe().GetAppById(source_app_id);
  EXPECT_FALSE(source_app->pending_migration_info().has_value());
}

}  // namespace

}  // namespace web_app
