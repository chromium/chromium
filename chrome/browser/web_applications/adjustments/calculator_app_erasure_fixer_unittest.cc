// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/calculator_app_erasure_fixer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class CalculatorAppErasureFixerTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  void StartExtensionsAndWebApps() {
    service()->Init();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void MarkWebAppUserUninstalled() {
    web_app::UserUninstalledPreinstalledWebAppPrefs web_prefs(
        profile()->GetPrefs());
    web_prefs.Add(web_app::kCalculatorAppId, /*install_urls=*/{});
  }

  void MarkChromeAppUserUninstalled() {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                "extensions.external_uninstalls");
    update->Append(extension_misc::kCalculatorAppId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kWebAppCalculatorAppErasureFixer};

  std::unique_ptr<CalculatorAppErasureFixer> fixer_;
};

TEST_F(CalculatorAppErasureFixerTest, ApplyFix) {
  base::HistogramTester histograms;

  // Mark both types of app as user uninstalled.
  MarkChromeAppUserUninstalled();
  MarkWebAppUserUninstalled();

  // CalculatorAppErasureFixer will run automatically once Web apps and Chrome
  // apps are initialized.
  StartExtensionsAndWebApps();

  histograms.ExpectBucketCount(
      kHistogramWebAppCalculatorAppErasureScanResult,
      CalculatorAppErasureFixer::ScanResult::kFixApplied, 1);

  web_app::UserUninstalledPreinstalledWebAppPrefs web_prefs(
      profile()->GetPrefs());
  EXPECT_FALSE(web_prefs.DoesAppIdExist(web_app::kCalculatorAppId));
  EXPECT_FALSE(extensions::IsExternalExtensionUninstalled(
      profile(), extension_misc::kCalculatorAppId));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      kWebAppCalculatorAppErasureFixAppliedPref));
}

TEST_F(CalculatorAppErasureFixerTest, FixAlreadyApplied) {
  base::HistogramTester histograms;

  // Mark both types of app as user uninstalled.
  MarkChromeAppUserUninstalled();
  MarkWebAppUserUninstalled();

  // Mark the fix as having been applied previously.
  profile()->GetPrefs()->SetBoolean(kWebAppCalculatorAppErasureFixAppliedPref,
                                    true);

  StartExtensionsAndWebApps();

  histograms.ExpectBucketCount(kHistogramWebAppCalculatorAppErasureScanResult,
                               CalculatorAppErasureFixer::ScanResult::
                                   kFixAlreadyAppliedAndWantedToApplyAgain,
                               1);

  // No changes should be made to user-uninstall prefs.
  web_app::UserUninstalledPreinstalledWebAppPrefs web_prefs(
      profile()->GetPrefs());
  EXPECT_TRUE(web_prefs.DoesAppIdExist(web_app::kCalculatorAppId));
  EXPECT_TRUE(extensions::IsExternalExtensionUninstalled(
      profile(), extension_misc::kCalculatorAppId));
}

TEST_F(CalculatorAppErasureFixerTest, AppsNotUserUninstalled) {
  base::HistogramTester histograms;

  StartExtensionsAndWebApps();

  histograms.ExpectBucketCount(
      kHistogramWebAppCalculatorAppErasureScanResult,
      CalculatorAppErasureFixer::ScanResult::kBothAppsNotUserUninstalled, 1);
}

TEST_F(CalculatorAppErasureFixerTest, WebAppNotUserUninstalled) {
  base::HistogramTester histograms;

  // Mark only the Chrome app as user uninstalled.
  MarkChromeAppUserUninstalled();

  StartExtensionsAndWebApps();

  histograms.ExpectBucketCount(
      kHistogramWebAppCalculatorAppErasureScanResult,
      CalculatorAppErasureFixer::ScanResult::kWebAppNotUserUninstalled, 1);
  // No change should be made to the Chrome app pref.
  EXPECT_TRUE(extensions::IsExternalExtensionUninstalled(
      profile(), extension_misc::kCalculatorAppId));
}

TEST_F(CalculatorAppErasureFixerTest, ChromeAppNotUserUninstalled) {
  base::HistogramTester histograms;

  // Mark only the Web app as user uninstalled.
  MarkWebAppUserUninstalled();

  StartExtensionsAndWebApps();

  histograms.ExpectBucketCount(
      kHistogramWebAppCalculatorAppErasureScanResult,
      CalculatorAppErasureFixer::ScanResult::kChromeAppNotUserUninstalled, 1);
  // No change should be made to the Web app pref.
  web_app::UserUninstalledPreinstalledWebAppPrefs web_prefs(
      profile()->GetPrefs());
  EXPECT_TRUE(web_prefs.DoesAppIdExist(web_app::kCalculatorAppId));
}

}  // namespace web_app
