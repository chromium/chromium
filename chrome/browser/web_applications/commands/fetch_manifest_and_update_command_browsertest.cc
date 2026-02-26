// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

class FetchManifestAndUpdateCommandTest : public WebAppBrowserTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_https_test_server().Start());
    WebAppBrowserTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(FetchManifestAndUpdateCommandTest, BasicUpdate) {
  webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
      embedded_https_test_server().GetURL("/web_apps/basic.html"));
  GURL app_url = embedded_https_test_server().GetURL("/web_apps/basic.html");
  webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);

  EXPECT_EQ("Basic web app",
            provider().registrar_unsafe().GetAppShortName(app_id));

  base::test::TestFuture<FetchManifestAndUpdateCompletionInfo> future;
  provider().scheduler().FetchManifestAndUpdate(
      embedded_https_test_server().GetURL(
          "/web_apps/get_manifest.html?basic_new_name.json"),
      manifest_id, /*previous_time_for_silent_icon_update=*/std::nullopt,
      /*force_trusted_silent_update=*/true, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(FetchManifestAndUpdateResult::kSuccess, future.Get().result);
  EXPECT_EQ("Basic web app 2",
            provider().registrar_unsafe().GetAppShortName(app_id));
}

class FetchManifestAndUpdateCommandMigrationTest
    : public FetchManifestAndUpdateCommandTest {
 public:
  FetchManifestAndUpdateCommandMigrationTest() {
    feature_list_.InitWithFeatures({blink::features::kWebAppMigrationApi,
                                    features::kWebAppPredictableAppUpdating},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FetchManifestAndUpdateCommandMigrationTest,
                       CheckMigrateFromTriggersUpdate) {
  Browser* app_browser = InstallWebAppFromPageGetBrowser(
      browser(), embedded_https_test_server().GetURL(
                     "/web_apps/migration/migrate_from/suggest.html"));
  webapps::AppId app_a_id = AppBrowserController::From(app_browser)->app_id();

  GURL app_b_url = embedded_https_test_server().GetURL(
      "/web_apps/migration/migrate_to/update_trigger_no_update.html");

  base::HistogramTester histogram_tester;

  base::test::TestFuture<void> manifest_seen_future;
  provider().manifest_update_manager().SetLoadFinishedCallbackForTesting(
      manifest_seen_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_b_url));

  EXPECT_TRUE(manifest_seen_future.Wait());

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // We expect success (no update detected, as App A hasn't changed).
  histogram_tester.ExpectBucketCount(
      "WebApp.FetchManifestAndUpdate.Result",
      FetchManifestAndUpdateResult::kSuccessNoUpdateDetected, 1);

  const WebApp* app_a = provider().registrar_unsafe().GetAppById(app_a_id);
  EXPECT_FALSE(app_a->pending_update_info().has_value());
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndUpdateCommandMigrationTest,
                       CheckMigrateFromTriggersUpdate_WithUpdate) {
  Browser* app_browser = InstallWebAppFromPageGetBrowser(
      browser(), embedded_https_test_server().GetURL(
                     "/web_apps/migration/migrate_from/suggest.html"));
  webapps::AppId app_a_id = AppBrowserController::From(app_browser)->app_id();

  GURL app_b_url = embedded_https_test_server().GetURL(
      "/web_apps/migration/migrate_to/update_trigger_with_update.html");

  base::HistogramTester histogram_tester;

  base::test::TestFuture<void> manifest_seen_future;
  provider().manifest_update_manager().SetLoadFinishedCallbackForTesting(
      manifest_seen_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_b_url));

  EXPECT_TRUE(manifest_seen_future.Wait());

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // We expect success. Since force_trusted_silent_update is false, the identity
  // change (name change) should be recorded as a pending update instead of
  // being applied immediately.
  histogram_tester.ExpectBucketCount("WebApp.FetchManifestAndUpdate.Result",
                                     FetchManifestAndUpdateResult::kSuccess, 1);

  EXPECT_EQ("Migrate From",
            provider().registrar_unsafe().GetAppShortName(app_a_id));
  const WebApp* app_a = provider().registrar_unsafe().GetAppById(app_a_id);
  ASSERT_TRUE(app_a->pending_update_info().has_value());
  EXPECT_EQ(app_a->pending_update_info()->name(), "Migrate From Updated Name");
}

}  // namespace web_app
