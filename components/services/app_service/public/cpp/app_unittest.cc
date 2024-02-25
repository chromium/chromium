// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_types.h"
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

template <>
std::pair<base::Value::Dict, base::Value::Dict> TestValue() {
  base::Value::Dict dict1;
  dict1.Set("vm_name", "vm_name_value");
  base::Value::Dict dict2;
  dict2.Set("container_name", "container_name_value");
  return {std::move(dict1), std::move(dict2)};
}

bool IsEqual(AppPtr app1, AppPtr app2) {
  std::vector<AppPtr> apps1;
  apps1.push_back(std::move(app1));

  std::vector<AppPtr> apps2;
  apps2.push_back(std::move(app2));

  return IsEqual(apps1, apps2);
}

template <typename T>
void VerifyOptionalValue(std::optional<T> App::*field) {
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

using AppTest = testing::Test;

TEST_F(AppTest, EmptyAppsIsEqual) {
  std::vector<AppPtr> apps1;
  std::vector<AppPtr> apps2;
  EXPECT_TRUE(IsEqual(apps1, apps2));
}

TEST_F(AppTest, VerifyAppsIsEqualForEmptyOptionalValues) {
  AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
  AppPtr app2 = app1->Clone();
  EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
}

TEST_F(AppTest, VerifyAppsIsEqualForReadiness) {
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

TEST_F(AppTest, VerifyAppsIsEqualForName) {
  VerifyOptionalValue(&App::name);
}

TEST_F(AppTest, VerifyAppsIsEqualForShortName) {
  VerifyOptionalValue(&App::short_name);
}

TEST_F(AppTest, VerifyAppsIsEqualForPublisherId) {
  VerifyOptionalValue(&App::publisher_id);
}

TEST_F(AppTest, VerifyAppsIsEqualForDescription) {
  VerifyOptionalValue(&App::description);
}

TEST_F(AppTest, VerifyAppsIsEqualForVersion) {
  VerifyOptionalValue(&App::version);
}

TEST_F(AppTest, VerifyAppsIsEqualForAdditionalSearchTerms) {
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

TEST_F(AppTest, VerifyAppsIsEqualForIconKey) {
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

TEST_F(AppTest, VerifyAppsIsEqualForLastLaunchTime) {
  VerifyOptionalValue(&App::last_launch_time);
}

TEST_F(AppTest, VerifyAppsIsEqualForInstallTime) {
  VerifyOptionalValue(&App::install_time);
}

TEST_F(AppTest, VerifyAppsIsEqualForPermissions) {
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

TEST_F(AppTest, VerifyAppsIsEqualForInstallReason) {
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

TEST_F(AppTest, VerifyAppsIsEqualForInstallSource) {
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

TEST_F(AppTest, VerifyAppsIsEqualForPolicyIds) {
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

TEST_F(AppTest, VerifyAppsIsEqualForIsPlatformApp) {
  VerifyOptionalValue(&App::is_platform_app);
}

TEST_F(AppTest, VerifyAppsIsEqualForRecommendable) {
  VerifyOptionalValue(&App::recommendable);
}

TEST_F(AppTest, VerifyAppsIsEqualForSearchable) {
  VerifyOptionalValue(&App::searchable);
}

TEST_F(AppTest, VerifyAppsIsEqualForShowInLauncher) {
  VerifyOptionalValue(&App::show_in_launcher);
}

TEST_F(AppTest, VerifyAppsIsEqualForShowInShelf) {
  VerifyOptionalValue(&App::show_in_shelf);
}

TEST_F(AppTest, VerifyAppsIsEqualForShowInSearch) {
  VerifyOptionalValue(&App::show_in_search);
}

TEST_F(AppTest, VerifyAppsIsEqualForShowInManagement) {
  VerifyOptionalValue(&App::show_in_management);
}

TEST_F(AppTest, VerifyAppsIsEqualForHandlesIntents) {
  VerifyOptionalValue(&App::handles_intents);
}

TEST_F(AppTest, VerifyAppsIsEqualForAllowUninstall) {
  VerifyOptionalValue(&App::allow_uninstall);
}

TEST_F(AppTest, VerifyAppsIsEqualForHasBadge) {
  VerifyOptionalValue(&App::has_badge);
}

TEST_F(AppTest, VerifyAppsIsEqualForPaused) {
  VerifyOptionalValue(&App::paused);
}

TEST_F(AppTest, VerifyAppsIsEqualForIntentFilters) {
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

TEST_F(AppTest, VerifyAppsIsEqualForResizeLocked) {
  VerifyOptionalValue(&App::resize_locked);
}

TEST_F(AppTest, VerifyAppsIsEqualForWindowMode) {
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

TEST_F(AppTest, VerifyAppsIsEqualForRunOnOsLogin) {
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

TEST_F(AppTest, VerifyAppsIsEqualForAppSizeInBytes) {
  VerifyOptionalValue(&App::app_size_in_bytes);
}

TEST_F(AppTest, VerifyAppsIsEqualForDataSizeInBytes) {
  VerifyOptionalValue(&App::data_size_in_bytes);
}

TEST_F(AppTest, VerifyAppsIsEqualForSupportedLocales) {
  // Verify the app is equal with the same `supported_locales`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    app1->supported_locales = {"C"};
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `supported_locales`.
  {
    AppPtr app1 = std::make_unique<App>(kAppType, kAppId);
    AppPtr app2 = app1->Clone();
    app1->supported_locales = {"C"};
    app2->supported_locales = {"B"};
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTest, VerifyAppsIsEqualForSelectedLocale) {
  VerifyOptionalValue(&App::selected_locale);
}

TEST_F(AppTest, VerifyAppsIsEqualForExtra) {
  VerifyOptionalValue(&App::extra);
}

}  // namespace apps
