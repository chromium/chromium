// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

class AppListSortBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  AppListSortBrowserTest() = default;
  AppListSortBrowserTest(const AppListSortBrowserTest&) = delete;
  AppListSortBrowserTest& operator=(const AppListSortBrowserTest&) = delete;
  ~AppListSortBrowserTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kProductivityLauncher, ash::features::kLauncherAppSort},
        /*disabled_features=*/{});
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Since the ProductivityLauncher flag is enabled, the sort buttons will
    // only be shown in tablet mode.
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();

    // Shows the app list which is initially behind a window in tablet mode.
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::TOGGLE_APP_LIST_FULLSCREEN, {});

    const int default_app_count = app_list_test_api_.GetTopListItemCount();

    if (base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport)) {
      // Assume that there are three default apps, one being the Lacros browser.
      ASSERT_EQ(3, app_list_test_api_.GetTopListItemCount());
    } else {
      // Assume that there are two default apps.
      ASSERT_EQ(2, app_list_test_api_.GetTopListItemCount());
    }

    app1_id_ = LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
    ASSERT_FALSE(app1_id_.empty());
    app2_id_ = LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
    ASSERT_FALSE(app2_id_.empty());
    // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
    // app in this test.
    app3_id_ = LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
    ASSERT_FALSE(app3_id_.empty());
    EXPECT_EQ(default_app_count + 3, app_list_test_api_.GetTopListItemCount());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  // Returns a list of app ids (excluding the default installed apps) following
  // the ordinal increasing order.
  std::vector<std::string> GetAppIdsInOrdinalOrder() {
    AppListModelUpdater* model_updater =
        test::GetModelUpdater(AppListClientImpl::GetInstance());
    std::vector<std::string> ids{app1_id_, app2_id_, app3_id_};
    std::sort(ids.begin(), ids.end(),
              [model_updater](const std::string& id1, const std::string& id2) {
                return model_updater->FindItem(id1)->position().LessThan(
                    model_updater->FindItem(id2)->position());
              });
    return ids;
  }

  ash::AppListTestApi app_list_test_api_;
  std::string app1_id_;
  std::string app2_id_;
  std::string app3_id_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the apps on the root launcher page can be arranged in the
// (reverse) alphabetical order.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, AlphabeticSort) {
  // Sort apps with the alphabetical order.
  event_generator_->GestureTapAt(
      app_list_test_api_
          .GetViewForAppListSort(ash::AppListSortOrder::kNameAlphabetical)
          ->GetBoundsInScreen()
          .CenterPoint());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Sort apps with the reverse alphabetical order.
  event_generator_->GestureTapAt(
      app_list_test_api_
          .GetViewForAppListSort(
              ash::AppListSortOrder::kNameReverseAlphabetical)
          ->GetBoundsInScreen()
          .CenterPoint());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));
}

// Verifies that the apps in a folder can be arranged in the (reverse)
// alphabetical order.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, AlphabeticSortFolderItems) {
  // Move apps to one folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});

  // Sort apps with the alphabetical order.
  event_generator_->GestureTapAt(
      app_list_test_api_
          .GetViewForAppListSort(ash::AppListSortOrder::kNameAlphabetical)
          ->GetBoundsInScreen()
          .CenterPoint());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Sort apps with the reverse alphabetical order.
  event_generator_->GestureTapAt(
      app_list_test_api_
          .GetViewForAppListSort(
              ash::AppListSortOrder::kNameReverseAlphabetical)
          ->GetBoundsInScreen()
          .CenterPoint());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));
}
