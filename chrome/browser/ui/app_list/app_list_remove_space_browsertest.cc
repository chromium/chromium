// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "content/public/test/browser_test.h"

// Verifies the compatibility between the environment with
// kLauncherRemoveEmptySpace enabled and the one with the feature flag disabled.
class AppListRemoveSpaceSyncCompatibilityTest
    : public extensions::ExtensionBrowserTest {
 public:
  AppListRemoveSpaceSyncCompatibilityTest() = default;
  AppListRemoveSpaceSyncCompatibilityTest(
      const AppListRemoveSpaceSyncCompatibilityTest&) = delete;
  AppListRemoveSpaceSyncCompatibilityTest& operator=(
      const AppListRemoveSpaceSyncCompatibilityTest&) = delete;
  ~AppListRemoveSpaceSyncCompatibilityTest() override = default;

 protected:
  void SetUp() override {
    // Enable the feature flag to remove extra spaces if the pre count is one.
    if (GetTestPreCount() == 1) {
      feature_list_.InitAndEnableFeature(ash::features::kProductivityLauncher);
    } else {
      feature_list_.InitAndDisableFeature(ash::features::kProductivityLauncher);
    }
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();

    // Run the test in tablet mode, so app list shows paged apps grid view when
    // the productivity launcher is enabled.
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    // Create the app list view by triggering the accelerator to show it.
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  }

  ash::AppListTestApi app_list_test_api_;
  base::test::ScopedFeatureList feature_list_;
};

// Configure the launcher with the feature flag disabled.
IN_PROC_BROWSER_TEST_F(AppListRemoveSpaceSyncCompatibilityTest,
                       PRE_PRE_Basics) {
  // Assume that the feature flag is disabled.
  ASSERT_FALSE(ash::features::IsProductivityLauncherEnabled());

  // Assume that there are two default apps.
  ASSERT_EQ(2, app_list_test_api_.GetTopListItemCount());

  // Add two apps.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Totally four items because two added apps besides two default apps.
  EXPECT_EQ(4, app_list_test_api_.GetTopListItemCount());

  // Add one page break item after app1.
  app_list_test_api_.AddPageBreakItemAfterId(app1_id);
  EXPECT_EQ(5, app_list_test_api_.GetTopListItemCount());
}

// Restart Chrome with the feature enabled.
IN_PROC_BROWSER_TEST_F(AppListRemoveSpaceSyncCompatibilityTest, PRE_Basics) {
  ASSERT_TRUE(ash::features::IsProductivityLauncherEnabled());

  // Because empty spaces are removed, there should be only one page.
  EXPECT_EQ(1, app_list_test_api_.GetPaginationModel()->total_pages());
  EXPECT_EQ(4, app_list_test_api_.GetTopListItemCount());
}

// Restart Chrome with the feature disabled.
IN_PROC_BROWSER_TEST_F(AppListRemoveSpaceSyncCompatibilityTest, Basics) {
  // The flag to remove empty spaces is turned off so there should be two pages.
  EXPECT_EQ(2, app_list_test_api_.GetPaginationModel()->total_pages());
  EXPECT_EQ(5, app_list_test_api_.GetTopListItemCount());
}
