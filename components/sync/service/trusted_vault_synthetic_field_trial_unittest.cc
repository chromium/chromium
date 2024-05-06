// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

MATCHER(IsNotValid, "") {
  return !arg.is_valid();
}

MATCHER_P(IsValidWithName, expected_name, "") {
  return arg.is_valid() && arg.name() == expected_name;
}

TEST(TrustedVaultSyntheticFieldTrialTest, ShouldBuildInvalidGroup) {
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::
                      AUTO_UPGRADE_EXPERIMENT_GROUP_UNSPECIFIED,
                  /*cohort_id=*/1),
              IsNotValid());
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::CONTROL,
                  /*cohort_id=*/0),
              IsNotValid());
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::CONTROL,
                  /*cohort_id=*/-1),
              IsNotValid());
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::CONTROL,
                  /*cohort_id=*/101),
              IsNotValid());
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldBuildInvalidGroupFromProtoDefaults) {
  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo debug_info;
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  debug_info.auto_upgrade_experiment_group(),
                  debug_info.auto_upgrade_cohort_id()),
              IsNotValid());
}

TEST(TrustedVaultSyntheticFieldTrialTest, ShouldGetValidGroupName) {
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::TREATMENT,
                  /*cohort_id=*/5),
              IsValidWithName("Treatment_5"));
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::CONTROL,
                  /*cohort_id=*/6),
              IsValidWithName("Control_6"));
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::VALIDATION,
                  /*cohort_id=*/7),
              IsValidWithName("Validation_7"));
}

}  // namespace

}  // namespace syncer
