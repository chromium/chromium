// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include "components/sync/protocol/nigori_specifics.pb.h"
#include "google_apis/gaia/gaia_id.h"
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

sync_pb::TrustedVaultAutoUpgradeExperimentGroup BuildTestProto(
    int cohort,
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type,
    int type_index) {
  sync_pb::TrustedVaultAutoUpgradeExperimentGroup proto;
  proto.set_cohort(cohort);
  proto.set_type(type);
  proto.set_type_index(type_index);
  return proto;
}

TEST(TrustedVaultSyntheticFieldTrialTest, ShouldBuildInvalidGroup) {
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/1,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TYPE_UNSPECIFIED,
          /*type_index=*/0)),
      IsNotValid());
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/0,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0)),
      IsNotValid());
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/-1,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0)),
      IsNotValid());
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/101,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0)),
      IsNotValid());
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/-1)),
      IsNotValid());
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/51)),
      IsNotValid());
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldBuildInvalidGroupFromProtoDefaults) {
  EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
                  sync_pb::TrustedVaultAutoUpgradeExperimentGroup()),
              IsNotValid());
}

TEST(TrustedVaultSyntheticFieldTrialTest, ShouldGetValidGroupName) {
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/5,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TREATMENT,
          /*type_index=*/0)),
      IsValidWithName("Cohort5_Treatment"));
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0)),
      IsValidWithName("Cohort6_Control"));
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/7,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION,
          /*type_index=*/0)),
      IsValidWithName("Cohort7_Validation"));
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldGetValidGroupNameWithTypeIndex) {
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/5,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TREATMENT,
          /*type_index=*/1)),
      IsValidWithName("Cohort5_Treatment1"));
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/2)),
      IsValidWithName("Cohort6_Control2"));
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/7,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION,
          /*type_index=*/3)),
      IsValidWithName("Cohort7_Validation3"));
  EXPECT_THAT(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          /*cohort=*/8,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION,
          /*type_index=*/50)),
      IsValidWithName("Cohort8_Validation50"));
}

}  // namespace

}  // namespace syncer
