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
#include "components/desks_storage/core/desk_test_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

  void WaitForAdminTemplateService() {
    auto* admin_template_service = GetAdminService();
    if (!admin_template_service) {
      return;
    }
    while (!admin_template_service->IsReady()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(
        ash::prefs::kAppLaunchAutomation);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    admin_template_service_ = std::make_unique<AdminTemplateService>(
        temp_dir_.GetPath(), account_id_, &pref_service_);

    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
    WaitForAdminTemplateService();
    testing::Test::SetUp();
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
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}

TEST_F(AdminTemplateServiceTest, AppliesModifiedPolicySettingCorrectly) {
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));
  SetPrefValue(ParsePolicyFromString(
      desk_test_util::kAdminTemplatePolicyWithOneTemplate));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 1UL);
}

TEST_F(AdminTemplateServiceTest, AppliesEmptyPolicySettingCorrectly) {
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));
  SetEmptyPrefValue();

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 0UL);
}

TEST_F(AdminTemplateServiceTest, AppliesAdditionalPolicySettingCorrectly) {
  SetPrefValue(ParsePolicyFromString(
      desk_test_util::kAdminTemplatePolicyWithOneTemplate));
  SetPrefValue(ParsePolicyFromString(desk_test_util::kAdminTemplatePolicy));

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}

}  // namespace desks_storage
