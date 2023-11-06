// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr AppType kAppType = AppType::kArc;
constexpr char kAppId[] = "abc";

template <typename T>
std::pair<T, T> TestValue();

template <>
std::pair<std::string, std::string> TestValue() {
  return {"apple", "banana"};
}

template <>
std::pair<bool, bool> TestValue() {
  return {true, false};
}

template <>
std::pair<base::Time, base::Time> TestValue() {
  return {base::Time::FromSecondsSinceUnixEpoch(1000),
          base::Time::FromSecondsSinceUnixEpoch(2000)};
}

template <>
std::pair<uint64_t, uint64_t> TestValue() {
  return {100, 200};
}

bool IsEqual(AppPtr app1, AppPtr app2) {
  std::vector<AppPtr> apps1;
  apps1.push_back(std::move(app1));

  std::vector<AppPtr> apps2;
  apps2.push_back(std::move(app2));

  return IsEqual(apps1, apps2);
}

template <typename T>
void VerifyOptionalValue(absl::optional<T> App::*field) {
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1.get()->*field = TestValue<T>().first;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app2.get()->*field = TestValue<T>().first;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1.get()->*field = TestValue<T>().first;
    app2.get()->*field = TestValue<T>().second;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

}  // namespace

using AppTypesTest = testing::Test;

TEST_F(AppTypesTest, EmptyAppsIsEqual) {
  std::vector<AppPtr> apps1;
  std::vector<AppPtr> apps2;
  EXPECT_TRUE(IsEqual(apps1, apps2));
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForEmptyOptionalValues) {
  AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
  AppPtr app2 = app1->Clone();
  EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForReadiness) {
  // Verify the app is equal with the same `readiness`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->readiness = Readiness::kReady;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `readiness`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->readiness = Readiness::kReady;
    AppPtr app2 = app1->Clone();
    app2->readiness = Readiness::kUninstalledByUser;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForName) {
  VerifyOptionalValue(&App::name);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShortName) {
  VerifyOptionalValue(&App::short_name);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPublisherId) {
  VerifyOptionalValue(&App::publisher_id);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForDescription) {
  VerifyOptionalValue(&App::description);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForVersion) {
  VerifyOptionalValue(&App::version);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAdditionalSearchTerms) {
  // Verify the app is equal with the same `additional_search_terms`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->additional_search_terms = {"aaa"};
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `additional_search_terms`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->additional_search_terms = {"aaa"};
    app2->additional_search_terms = {"bbb"};
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIconKey) {
  // Verify the app is equal with the same `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->icon_key = IconKey();
    app1->icon_key->update_version = 100;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with an empty `icon_key` vs non-empty
  // `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app2->icon_key = IconKey();
    app2->icon_key->update_version = 100;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->icon_key = IconKey();
    app1->icon_key->update_version = 100;
    app2->icon_key = IconKey();
    app2->icon_key->update_version = 200;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForLastLaunchTime) {
  VerifyOptionalValue(&App::last_launch_time);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForInstallTime) {
  VerifyOptionalValue(&App::install_time);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPermissions) {
  // Verify the app is equal with the same `permissions`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, TriState::kAllow, /*is_managed=*/true));
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `permissions`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, TriState::kAllow, /*is_managed=*/true));
    app2->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, TriState::kAllow, /*is_managed=*/false));
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForInstallReason) {
  // Verify the app is equal with the same `install_reason`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->install_reason = InstallReason::kUser;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_reason`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->install_reason = InstallReason::kUser;
    app2->install_reason = InstallReason::kSystem;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForInstallSource) {
  // Verify the app is equal with the same `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->install_source = InstallSource::kPlayStore;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->install_source = InstallSource::kPlayStore;
    app2->install_source = InstallSource::kBrowser;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPolicyIds) {
  // Verify the app is equal with the same `policy_ids`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->policy_ids = {"policy1"};
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->policy_ids = {"policy1"};
    app2->policy_ids = {"policy2"};
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIsPlatformApp) {
  VerifyOptionalValue(&App::is_platform_app);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForRecommendable) {
  VerifyOptionalValue(&App::recommendable);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForSearchable) {
  VerifyOptionalValue(&App::searchable);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInLauncher) {
  VerifyOptionalValue(&App::show_in_launcher);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInShelf) {
  VerifyOptionalValue(&App::show_in_shelf);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInSearch) {
  VerifyOptionalValue(&App::show_in_search);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInManagement) {
  VerifyOptionalValue(&App::show_in_management);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForHandlesIntents) {
  VerifyOptionalValue(&App::handles_intents);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAllowUninstall) {
  VerifyOptionalValue(&App::allow_uninstall);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForHasBadge) {
  VerifyOptionalValue(&App::has_badge);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPaused) {
  VerifyOptionalValue(&App::paused);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIntentFilters) {
  IntentFilterPtr intent_filter1 = std::make_unique<IntentFilter>();
  intent_filter1->activity_name = "abc";

  IntentFilterPtr intent_filter2 = std::make_unique<IntentFilter>();
  intent_filter2->activity_name = "xyz";

  // Verify the app is equal with the same `intent_filter`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->intent_filters.push_back(intent_filter1->Clone());
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `intent_filter`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->intent_filters.push_back(intent_filter1->Clone());
    app2->intent_filters.push_back(intent_filter2->Clone());
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForResizeLocked) {
  VerifyOptionalValue(&App::resize_locked);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForWindowMode) {
  // Verify the app is equal with the same `window_mode`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->window_mode = WindowMode::kBrowser;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `window_mode`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->window_mode = WindowMode::kBrowser;
    app2->window_mode = WindowMode::kTabbedWindow;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForRunOnOsLogin) {
  // Verify the app is equal with the same `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with an empty `run_on_os_login` vs non-empty
  // `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, true);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAppSizeInBytes) {
  VerifyOptionalValue(&App::app_size_in_bytes);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForDataSizeInBytes) {
  VerifyOptionalValue(&App::data_size_in_bytes);
}

}  // namespace apps
