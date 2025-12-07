// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/test/browser_test.h"

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

  base::test::TestFuture<FetchManifestAndUpdateResult> future;
  provider().scheduler().FetchManifestAndUpdate(
      embedded_https_test_server().GetURL(
          "/web_apps/get_manifest.html?basic_new_name.json"),
      manifest_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(FetchManifestAndUpdateResult::kSuccess, future.Get());
  EXPECT_EQ("Basic web app 2",
            provider().registrar_unsafe().GetAppShortName(app_id));
}

}  // namespace web_app
