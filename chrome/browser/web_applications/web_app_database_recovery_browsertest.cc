// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "url/gurl.h"

namespace web_app {

constexpr std::string_view kAppUrl = "https://example.com/app";

class WebAppDatabaseRecoveryBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppDatabaseRecoveryBrowserTest()
      : skip_message_box_auto_reset_(
            &chrome::internal::g_should_skip_message_box_for_test,
            true) {}
  ~WebAppDatabaseRecoveryBrowserTest() override = default;

  base::HistogramTester histogram_tester_;

 private:
  base::AutoReset<bool> skip_message_box_auto_reset_;
};

IN_PROC_BROWSER_TEST_F(WebAppDatabaseRecoveryBrowserTest,
                       PRE_RecoverFromDowngrade) {
  // Install a web app to ensure database has content.
  auto app_id = test::InstallDummyWebApp(browser()->profile(), "Example App",
                                         GURL(kAppUrl));

  // Access database and set version to a future version that should mean a
  // downgrade was detected in the next test, and thus data cleared.
  WebAppDatabase* database =
      provider().sync_bridge_unsafe().GetDatabaseForTesting();
  ASSERT_TRUE(database);

  base::RunLoop version_loop;
  database->SetDatabaseVersionForTesting(
      WebAppDatabase::GetCurrentDatabaseVersion() + 1,
      version_loop.QuitClosure());
  version_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppDatabaseRecoveryBrowserTest,
                       RecoverFromDowngrade) {
  // Verify that the app we installed in PRE_ test is gone because database was
  // reset.
  EXPECT_EQ(provider().registrar_unsafe().GetAppById(
                GenerateAppId(std::nullopt, GURL(kAppUrl))),
            nullptr);
  histogram_tester_.ExpectBucketCount("WebApp.Database.OpenResult",
                                      WebAppDatabaseOpenResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      "WebApp.Database.OpenResult",
      WebAppDatabaseOpenResult::kDowngradeDetected, 1);
}

}  // namespace web_app
