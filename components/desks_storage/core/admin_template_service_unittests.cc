// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/admin_template_service.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {
base::Value ParsePolicyFromString(base::StringPiece policy) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(policy);

  CHECK(parsed_json.has_value());
  CHECK(parsed_json->is_list());

  return std::move(parsed_json.value());
}

}  // namespace

class AdminTemplateServiceTest : public testing::Test {
 public:
  AdminTemplateServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(std::make_unique<apps::AppRegistryCache>()) {}
  AdminTemplateServiceTest(const AdminTemplateServiceTest&) = delete;
  AdminTemplateServiceTest& operator=(const AdminTemplateServiceTest&) = delete;
  ~AdminTemplateServiceTest() override = default;

  AdminTemplateService* GetAdminService() {
    return admin_template_service_.get();
  }

  void WaitForAdminTemplateServiceModel() {
    auto* admin_template_service = GetAdminService();
    if (!admin_template_service) {
      return;
    }
    while (!admin_template_service->GetFullDeskModel()->IsReady()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(
        ash::prefs::kAppLaunchAutomation);

    // Add an empty apps registry cache, see why in comment over
    // `SetUpAppsCache`
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             cache_.get());

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    admin_template_service_ = std::make_unique<AdminTemplateService>(
        temp_dir_.GetPath(), account_id_, &pref_service_);

    WaitForAdminTemplateServiceModel();
    testing::Test::SetUp();
  }

  // Because on most platforms calling `ReinitialzeForTesting` on an apps cache
  // will not reset the initialized app types set we have to defer the
  // initialization of the apps cache in the test
  // `WaitsForAppsCacheBeforeParsingPolicy` so this method must be called at
  // the beginning of all other tests in this file.
  //
  // see: Components/service/app_service/public/cpp/app_registry_cache.cc
  // method: `ReinitializeForTesting`
  void SetUpAppsCache() {
    desk_test_util::PopulateAdminTestAppRegistryCache(account_id_,
                                                      cache_.get());
    task_environment_.RunUntilIdle();
  }

  void SetPrefValue(const base::Value& value) {
    pref_service_.SetManagedPref(ash::prefs::kAppLaunchAutomation,
                                 value.Clone());

    task_environment_.RunUntilIdle();
  }

  void SetEmptyPrefValue() {
    pref_service_.SetManagedPref(ash::prefs::kAppLaunchAutomation,
                                 base::Value::List());

    task_environment_.RunUntilIdle();
  }

  void ResetAdminTemplateService() {
    admin_template_service_ = std::make_unique<AdminTemplateService>(
        temp_dir_.GetPath(), account_id_, &pref_service_);
    WaitForAdminTemplateServiceModel();
  }

  apps::AppRegistryCache* GetAppsCache() { return cache_.get(); }

  AccountId& GetAccountId() { return account_id_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AdminTemplateService> admin_template_service_ = nullptr;
  PrefValueMap pref_map_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
};

TEST_F(AdminTemplateServiceTest, AppliesPolicySettingCorrectly) {
  SetUpAppsCache();
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}

TEST_F(AdminTemplateServiceTest, AppliesModifiedPolicySettingCorrectly) {
  SetUpAppsCache();
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));
  SetPrefValue(ParsePolicyFromString(
      desk_test_util::kAdminTemplatePolicyWithOneTemplate));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 1UL);
}

TEST_F(AdminTemplateServiceTest, AppliesEmptyPolicySettingCorrectly) {
  SetUpAppsCache();
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));
  SetEmptyPrefValue();

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 0UL);
}

TEST_F(AdminTemplateServiceTest, AppliesAdditionalPolicySettingCorrectly) {
  SetUpAppsCache();
  SetPrefValue(ParsePolicyFromString(
      desk_test_util::kAdminTemplatePolicyWithOneTemplate));
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}

TEST_F(AdminTemplateServiceTest, WaitsForAppsCacheBeforeParsingPolicy) {
  // Set up test environment such that  the admin template service will be
  // loaded before the apps cache has initialized the browser types.
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  // We will attempt to load the model when the preference is set, however we
  // will not load because the apps cache doesn't currently recognize the
  // browser ids.
  EXPECT_EQ(
      GetAdminService()->GetFullDeskModel()->GetAllEntries().entries.size(),
      0UL);

  // Now we populate the apps cache, which should in turn populate the
  // entries.
  SetUpAppsCache();

  auto all_entries_result =
      GetAdminService()->GetFullDeskModel()->GetAllEntries();

  EXPECT_EQ(all_entries_result.status, DeskModel::GetAllEntriesStatus::kOk);
  std::vector<const ash::DeskTemplate*>& entries = all_entries_result.entries;

  EXPECT_EQ(entries.size(), 2UL);

  // Finally we verify that the restore data is populated.
  for (const auto* entry : entries) {
    // When we support other types we will make this check more comprehensive.
    EXPECT_TRUE(entry->desk_restore_data()->HasBrowser());
  }
}

TEST_F(AdminTemplateServiceTest,
       WaitsToObserveAppsCacheUntilItsAddedToWrapper) {
  // Set up The environment such that there is currently no AppRegistryCache for
  // the environments account_id.  Set the pref value to ensure that it doesn't
  // attempt to parse anyways.
  apps::AppRegistryCacheWrapper& wrapper = apps::AppRegistryCacheWrapper::Get();
  wrapper.RemoveAppRegistryCache(GetAppsCache());
  ResetAdminTemplateService();
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  // We will attempt to load the model when the preference is set, however we
  // will not load because the apps cache is null.
  EXPECT_EQ(
      GetAdminService()->GetFullDeskModel()->GetAllEntries().entries.size(),
      0UL);

  // Add back in the cache and populate it.
  wrapper.AddAppRegistryCache(GetAccountId(), GetAppsCache());
  SetUpAppsCache();

  auto all_entries_result =
      GetAdminService()->GetFullDeskModel()->GetAllEntries();

  EXPECT_EQ(all_entries_result.status, DeskModel::GetAllEntriesStatus::kOk);
  std::vector<const ash::DeskTemplate*>& entries = all_entries_result.entries;

  EXPECT_EQ(entries.size(), 2UL);

  // Finally we verify that the restore data is populated.
  for (const auto* entry : entries) {
    // When we support other types we will make this check more comprehensive.
    EXPECT_TRUE(entry->desk_restore_data()->HasBrowser());
  }
}

}  // namespace desks_storage
