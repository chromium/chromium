// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extended_updates/test/mock_extended_updates_controller.h"
#include "chrome/browser/ash/extended_updates/test/scoped_extended_updates_controller.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ExtendedUpdatesBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ExtendedUpdatesBrowserTest() {
    set_test_loader_host(chrome::kChromeUIExtendedUpdatesDialogHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();

    // The ExtendedUpdates webui checks that the user is the owner before
    // allowing the page to open. That ownership check depends on encryption
    // keys being loaded, which happens asynchronously, so we need to
    // wait for it to finish loading before trying to open the page.
    WaitForIsOwner();
  }

  void WaitForIsOwner() {
    base::RunLoop run_loop;
    auto* owner_settings =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(owner_settings);
    owner_settings->IsOwnerAsync(base::BindLambdaForTesting(
        [&run_loop](bool is_owner) { run_loop.Quit(); }));
    run_loop.Run();
  }

  std::string GetTrigger(const std::string& suite = "",
                         const std::string& test = "") {
    if (suite.empty()) {
      return "mocha.run()";
    }
    if (test.empty()) {
      return base::StrCat({"runMochaSuite('", suite, "')"});
    }
    return base::StrCat({"runMochaTest('", suite, "', '", test, "')"});
  }

  void RunTestFile(const std::string& test_file,
                   const std::string& suite = "",
                   const std::string& test = "") {
    RunTest(base::StrCat({"chromeos/extended_updates/", test_file}),
            GetTrigger(suite, test));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kExtendedUpdatesOptInFeature};
};

IN_PROC_BROWSER_TEST_F(ExtendedUpdatesBrowserTest, AppTest) {
  RunTestFile("extended_updates_app_test.js", "<extended-updates> <app-test>");
}

IN_PROC_BROWSER_TEST_F(ExtendedUpdatesBrowserTest, DialogMetricsTest) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ash::extended_updates::ExtendedUpdatesDialog::Get());
  ash::extended_updates::ExtendedUpdatesDialog::Show();
  EXPECT_TRUE(ash::extended_updates::ExtendedUpdatesDialog::Get());
  histogram_tester.ExpectBucketCount(
      ash::kExtendedUpdatesDialogEventMetric,
      ash::ExtendedUpdatesDialogEvent::kDialogShown, 1);

  RunTestFile("extended_updates_app_test.js", "<extended-updates> <util>",
              "perform opt in");
  histogram_tester.ExpectBucketCount(
      ash::kExtendedUpdatesDialogEventMetric,
      ash::ExtendedUpdatesDialogEvent::kOptInConfirmed, 1);
}

IN_PROC_BROWSER_TEST_F(ExtendedUpdatesBrowserTest, NoShowDialogIfNotEligible) {
  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);
  EXPECT_CALL(mock_controller, IsOptInEligible(browser()->profile()))
      .WillOnce(testing::Return(false));

  EXPECT_FALSE(ash::extended_updates::ExtendedUpdatesDialog::Get());
  ash::extended_updates::ExtendedUpdatesDialog::Show();
  EXPECT_FALSE(ash::extended_updates::ExtendedUpdatesDialog::Get());
}

IN_PROC_BROWSER_TEST_F(ExtendedUpdatesBrowserTest, CloseDialogIfNotEligible) {
  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);
  EXPECT_CALL(mock_controller, IsOptInEligible(browser()->profile()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_FALSE(ash::extended_updates::ExtendedUpdatesDialog::Get());
  ash::extended_updates::ExtendedUpdatesDialog::Show();
  EXPECT_TRUE(ash::extended_updates::ExtendedUpdatesDialog::Get());

  EXPECT_CALL(mock_controller, IsOptInEligible(browser()->profile()))
      .WillOnce(testing::Return(false));
  ash::extended_updates::ExtendedUpdatesDialog::Show();

  ui_test_utils::CheckWaiter(
      base::BindRepeating([] {
        return bool(ash::extended_updates::ExtendedUpdatesDialog::Get());
      }),
      false, base::Milliseconds(1000))
      .Wait();
  EXPECT_FALSE(ash::extended_updates::ExtendedUpdatesDialog::Get());
}
