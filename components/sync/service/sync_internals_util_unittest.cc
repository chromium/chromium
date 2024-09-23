// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_internals_util.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer::sync_ui_util {

namespace {

// Helper function that allows using ASSERT_xxx inside because it returns void.
void FindStatWithNameImpl(const base::Value::Dict& strings,
                          const std::string& stat_name,
                          const base::Value** result) {
  const base::Value* details = strings.Find("details");
  ASSERT_TRUE(details);
  ASSERT_TRUE(details->is_list());

  for (const auto& details_entry : details->GetList()) {
    ASSERT_TRUE(details_entry.is_dict());

    const base::Value* data = details_entry.GetDict().Find("data");
    ASSERT_TRUE(data);
    ASSERT_TRUE(data->is_list());

    for (const auto& data_entry : data->GetList()) {
      ASSERT_TRUE(data_entry.is_dict());

      const base::Value* data_stat_name =
          data_entry.GetDict().Find("stat_name");
      const base::Value* data_stat_value =
          data_entry.GetDict().Find("stat_value");

      ASSERT_TRUE(data_stat_name);
      ASSERT_TRUE(data_stat_value);
      ASSERT_TRUE(data_stat_name->is_string());

      if (data_stat_name->GetString() == stat_name) {
        EXPECT_FALSE(*result) << "stat found more than once";
        *result = data_stat_value;
      }
    }
  }
}

const base::Value* FindStatWithName(const base::Value::Dict& strings,
                                    const std::string& stat_name) {
  const base::Value* result = nullptr;
  FindStatWithNameImpl(strings, stat_name, &result);
  return result;
}

TEST(SyncUIUtilTestAbout, ConstructAboutInformationWithUnrecoverableErrorTest) {
  TestSyncService service;
  service.SetHasUnrecoverableError(true);

  const base::Value::Dict strings = ConstructAboutInformation(
      IncludeSensitiveData(true), &service, std::string());

  EXPECT_TRUE(strings.Find("unrecoverable_error_detected"));
}

TEST(
    SyncUIUtilTestAbout,
    ConstructAboutInformationWithTrustedUnspecifiedVaultAutoUpgradeExperiment) {
  TestSyncService service;
  const base::Value::Dict strings = ConstructAboutInformation(
      IncludeSensitiveData(true), &service, std::string());
  const base::Value* auto_upgrade_experiment_group =
      FindStatWithName(strings, "Trusted Vault Auto Upgrade Group");

  ASSERT_TRUE(auto_upgrade_experiment_group);
  ASSERT_TRUE(auto_upgrade_experiment_group->is_string());
  EXPECT_EQ("Uninitialized", auto_upgrade_experiment_group->GetString());
}

TEST(SyncUIUtilTestAbout,
     ConstructAboutInformationWithTrustedSpecifiedVaultAutoUpgradeExperiment) {
  TestSyncService service;
  SyncStatus status;
  status.trusted_vault_debug_info.mutable_auto_upgrade_experiment_group()
      ->set_cohort(5);
  status.trusted_vault_debug_info.mutable_auto_upgrade_experiment_group()
      ->set_type(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION);
  status.trusted_vault_debug_info.mutable_auto_upgrade_experiment_group()
      ->set_type_index(9);

  service.SetDetailedSyncStatus(/*engine_available=*/true, status);

  const base::Value::Dict strings = ConstructAboutInformation(
      IncludeSensitiveData(true), &service, std::string());
  const base::Value* auto_upgrade_experiment_group =
      FindStatWithName(strings, "Trusted Vault Auto Upgrade Group");

  ASSERT_TRUE(auto_upgrade_experiment_group);
  ASSERT_TRUE(auto_upgrade_experiment_group->is_string());
  EXPECT_EQ("Cohort5_Validation9", auto_upgrade_experiment_group->GetString());
}

}  // namespace

}  // namespace syncer::sync_ui_util
