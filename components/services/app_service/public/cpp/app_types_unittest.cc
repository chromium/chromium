// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

#define VERIFY_OPTIONAL_STRING_VALUE(VALUE)                  \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    app1->VALUE = "banana";                                  \
    AppPtr app2 = app1->Clone();                             \
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));  \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app2->VALUE = "banana";                                  \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app1->VALUE = "apple";                                   \
    app2->VALUE = "banana";                                  \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }

#define VERIFY_OPTIONAL_BOOL_VALUE(VALUE)                    \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    app1->VALUE = true;                                      \
    AppPtr app2 = app1->Clone();                             \
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));  \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app2->VALUE = false;                                     \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app1->VALUE = true;                                      \
    app2->VALUE = false;                                     \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }

#define VERIFY_OPTIONAL_TIME_VALUE(VALUE)                    \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    app1->VALUE = base::Time::FromDoubleT(1000.0);           \
    AppPtr app2 = app1->Clone();                             \
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));  \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app2->VALUE = base::Time::FromDoubleT(1000.0);           \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app1->VALUE = base::Time::FromDoubleT(1000.0);           \
    app2->VALUE = base::Time::FromDoubleT(2000.0);           \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }

#define VERIFY_OPTIONAL_INT_VALUE(VALUE)                     \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    app1->VALUE = 100;                                       \
    AppPtr app2 = app1->Clone();                             \
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));  \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app2->VALUE = 200;                                       \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }                                                          \
  {                                                          \
    AppPtr app1 = std::make_unique<App>(app_type, app_id);   \
    AppPtr app2 = app1->Clone();                             \
    app1->VALUE = 100;                                       \
    app2->VALUE = 200;                                       \
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2))); \
  }

const AppType app_type = AppType::kArc;
const char app_id[] = "abc";

bool IsEqual(AppPtr app1, AppPtr app2) {
  std::vector<AppPtr> apps1;
  apps1.push_back(std::move(app1));

  std::vector<AppPtr> apps2;
  apps2.push_back(std::move(app2));

  return IsEqual(apps1, apps2);
}

}  // namespace

using AppTypesTest = testing::Test;

TEST_F(AppTypesTest, EmptyAppsIsEqual) {
  std::vector<AppPtr> apps1;
  std::vector<AppPtr> apps2;
  EXPECT_TRUE(IsEqual(apps1, apps2));
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForEmptyOptionalValues) {
  AppPtr app1 = std::make_unique<App>(app_type, app_id);
  AppPtr app2 = app1->Clone();
  EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForReadiness) {
  // Verify the app is equal with the same `readiness`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->readiness = Readiness::kReady;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `readiness`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->readiness = Readiness::kReady;
    AppPtr app2 = app1->Clone();
    app2->readiness = Readiness::kUninstalledByUser;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForName) {
  VERIFY_OPTIONAL_STRING_VALUE(name);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShortName) {
  VERIFY_OPTIONAL_STRING_VALUE(short_name);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPublisherId) {
  VERIFY_OPTIONAL_STRING_VALUE(publisher_id);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForDescription) {
  VERIFY_OPTIONAL_STRING_VALUE(description);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForVersion) {
  VERIFY_OPTIONAL_STRING_VALUE(version);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAdditionalSearchTerms) {
  // Verify the app is equal with the same `additional_search_terms`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->additional_search_terms = {"aaa"};
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `additional_search_terms`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->additional_search_terms = {"aaa"};
    app2->additional_search_terms = {"bbb"};
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIconKey) {
  // Verify the app is equal with the same `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->icon_key = IconKey(100, 0, 0);
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with an empty `icon_key` vs non-empty
  // `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app2->icon_key = IconKey(100, 0, 0);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `icon_key`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->icon_key = IconKey(100, 0, 0);
    app2->icon_key = IconKey(200, 0, 0);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForLastLaunchTime) {
  VERIFY_OPTIONAL_TIME_VALUE(last_launch_time);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForInstallTime) {
  VERIFY_OPTIONAL_TIME_VALUE(install_time);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPermissions) {
  // Verify the app is equal with the same `permissions`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, TriState::kAllow, /*is_managed=*/true));
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `permissions`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
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
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->install_reason = InstallReason::kUser;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_reason`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->install_reason = InstallReason::kUser;
    app2->install_reason = InstallReason::kSystem;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForInstallSource) {
  // Verify the app is equal with the same `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->install_source = InstallSource::kPlayStore;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->install_source = InstallSource::kPlayStore;
    app2->install_source = InstallSource::kBrowser;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPolicyIds) {
  // Verify the app is equal with the same `policy_ids`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->policy_ids = {"policy1"};
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `install_source`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->policy_ids = {"policy1"};
    app2->policy_ids = {"policy2"};
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIsPlatformApp) {
  VERIFY_OPTIONAL_BOOL_VALUE(is_platform_app);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForRecommendable) {
  VERIFY_OPTIONAL_BOOL_VALUE(recommendable);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForSearchable) {
  VERIFY_OPTIONAL_BOOL_VALUE(searchable);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInLauncher) {
  VERIFY_OPTIONAL_BOOL_VALUE(show_in_launcher);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInShelf) {
  VERIFY_OPTIONAL_BOOL_VALUE(show_in_shelf);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInSearch) {
  VERIFY_OPTIONAL_BOOL_VALUE(show_in_search);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForShowInManagement) {
  VERIFY_OPTIONAL_BOOL_VALUE(show_in_management);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForHandlesIntents) {
  VERIFY_OPTIONAL_BOOL_VALUE(handles_intents);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAllowUninstall) {
  VERIFY_OPTIONAL_BOOL_VALUE(allow_uninstall);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForHasBadge) {
  VERIFY_OPTIONAL_BOOL_VALUE(has_badge);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForPaused) {
  VERIFY_OPTIONAL_BOOL_VALUE(paused);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForIntentFilters) {
  IntentFilterPtr intent_filter1 = std::make_unique<IntentFilter>();
  intent_filter1->activity_name = "abc";

  IntentFilterPtr intent_filter2 = std::make_unique<IntentFilter>();
  intent_filter2->activity_name = "xyz";

  // Verify the app is equal with the same `intent_filter`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->intent_filters.push_back(intent_filter1->Clone());
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `intent_filter`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->intent_filters.push_back(intent_filter1->Clone());
    app2->intent_filters.push_back(intent_filter2->Clone());
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForResizeLocked) {
  VERIFY_OPTIONAL_BOOL_VALUE(resize_locked);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForWindowMode) {
  // Verify the app is equal with the same `window_mode`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->window_mode = WindowMode::kBrowser;
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `window_mode`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->window_mode = WindowMode::kBrowser;
    app2->window_mode = WindowMode::kTabbedWindow;
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForRunOnOsLogin) {
  // Verify the app is equal with the same `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    app1->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    AppPtr app2 = app1->Clone();
    EXPECT_TRUE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with an empty `run_on_os_login` vs non-empty
  // `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }

  // Verify the app is not equal with different `run_on_os_login`.
  {
    AppPtr app1 = std::make_unique<App>(app_type, app_id);
    AppPtr app2 = app1->Clone();
    app1->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, true);
    EXPECT_FALSE(IsEqual(std::move(app1), std::move(app2)));
  }
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForAppSizeInBytes) {
  VERIFY_OPTIONAL_INT_VALUE(app_size_in_bytes);
}

TEST_F(AppTypesTest, VerifyAppsIsEqualForDataSizeInBytes) {
  VERIFY_OPTIONAL_INT_VALUE(data_size_in_bytes);
}

}  // namespace apps
