// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_reporter.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_util.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class WebAppLauncherLogTest : public testing::Test {
 protected:
  WebAppLauncherLogTest() = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_EQ(ERROR_SUCCESS,
              key_.Create(HKEY_CURRENT_USER,
                          install_static::GetRegistryPath().c_str(),
                          KEY_CREATE_SUB_KEY | KEY_READ));
    ASSERT_TRUE(key_.Valid());
  }

  base::win::RegKey key_;

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

TEST_F(WebAppLauncherLogTest, Log) {
  const WebAppLauncherLaunchResult expected_value =
      WebAppLauncherLaunchResult::kError;

  // Log |expected_value| to the registry.
  LauncherLog launcher_log;
  ASSERT_FALSE(key_.HasValue(kPwaLauncherResult));
  launcher_log.Log(expected_value);
  ASSERT_TRUE(key_.HasValue(kPwaLauncherResult));

  // PWALauncherResult should have been set to |expected_value| in the registry.
  DWORD logged_value;
  EXPECT_EQ(ERROR_SUCCESS, key_.ReadValueDW(kPwaLauncherResult, &logged_value));
  EXPECT_EQ(expected_value,
            static_cast<WebAppLauncherLaunchResult>(logged_value));
}

TEST_F(WebAppLauncherLogTest, RecordPwaLauncherResult) {
  const WebAppLauncherLaunchResult expected_value =
      WebAppLauncherLaunchResult::kSuccess;

  // Log |expected_value| to the registry.
  LauncherLog launcher_log;
  ASSERT_FALSE(key_.HasValue(kPwaLauncherResult));
  launcher_log.Log(expected_value);
  ASSERT_TRUE(key_.HasValue(kPwaLauncherResult));

  // Record the logged value (at PWALauncherResult in the registry) to UMA.
  base::HistogramTester histogram_tester;
  RecordPwaLauncherResult();

  // |expected_value| should have been recorded to UMA.
  histogram_tester.ExpectUniqueSample("WebApp.Launcher.LaunchResult",
                                      expected_value, 1);

  // PWALauncherResult should have been deleted from the registry.
  EXPECT_FALSE(key_.HasValue(kPwaLauncherResult));
}

}  // namespace web_app
