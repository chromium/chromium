// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::FloatNear;
using testing::IsEmpty;

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

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldProduceDeterministicFloatBetweenZeroAndOneFromGaiaId) {
  // Tolerance used in case floating point operations may differ slightly across
  // platforms.
  const float kEpsilon = 1E-6f;

  // Expected values have been computed empirically. They are all between zero
  // and one as expected and appear to be uniformly distributed.
  struct {
    std::string gaia_id;
    std::string salt;
    float expected_value;
  } test_cases[] = {
      {"gaia_id_1", "salt1", 0.18181f},
      {"gaia_id_2", "salt1", 0.46258f},
      {"gaia_id_3", "salt1", 0.08043f},
      {"gaia_id_3", "salt2", 0.82853f},
      {"gaia_id_3", "salt3", 0.10936f},
      {"gaia_id_3", "", 0.76956f},
      // Specific value used in other tests (found empirically).
      {"gaia_id_49", "UserSampling", 0.00543f},
      {"gaia_id_49", "OnProfileLoadSampled", 0.05258f},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                    DeterministicFloatBetweenZeroAndOneFromGaiaIdForTest(
                        test_case.gaia_id, test_case.salt),
                FloatNear(test_case.expected_value, kEpsilon))
        << " for " << test_case.gaia_id << " and salt " << test_case.salt;
  }
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldSampleGaiaIdsWithTenPercentProbability) {
  const int kNumGaiaIdsToTest = 50;

  int num_sampled_gaia_ids = 0;
  for (int i = 0; i < kNumGaiaIdsToTest; i++) {
    if (TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
            ShouldSampleGaiaIdWithTenPercentProbabilityForTest(
                base::StringPrintf("gaia_id_%d", i))) {
      num_sampled_gaia_ids++;
    }
  }

  // About 10% of the gaia IDs should have returned true (i.e. should be
  // sampled). In this case, as empirically observed, it is off by one.
  EXPECT_THAT(num_sampled_gaia_ids, Eq(kNumGaiaIdsToTest / 10 + 1));
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldLogValidationMetricsUponOnProfileLoad) {
  const int kCohort = 6;     // Not relevant in this test.
  const int kTypeIndex = 7;  // Not relevant in this test.
  const auto control_group =
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          kCohort, sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          kTypeIndex));
  const auto validation_group =
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          kCohort, sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION,
          kTypeIndex));

  // String chosen from previous tests, given the convenient float value
  // computed from it.
  const std::string kGaiaId = "gaia_id_49";
  ASSERT_TRUE(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                  ShouldSampleGaiaIdWithTenPercentProbabilityForTest(kGaiaId));
  ASSERT_THAT(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                  DeterministicFloatBetweenZeroAndOneFromGaiaIdForTest(
                      kGaiaId, /*salt=*/"OnProfileLoadSampled"),
              FloatNear(0.05258f, 1E-6));

  // Control group.
  {
    base::HistogramTester tester;
    control_group.LogValidationMetricsUponOnProfileLoad(kGaiaId);

    // Metrics with account consistency record a deterministic bucket.
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V01",
        false, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V04",
        false, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V06",
        false, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V09",
        false, /*expected_bucket_count=*/1);
    // Thresholds larger than 0.05258f record `true`.
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V20",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V23",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V25",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V28",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V50",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V53",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V55",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V58",
        true, /*expected_bucket_count=*/1);

    // Metrics without account consistency are known to record one value, but
    // the value is random.
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C01_V01",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C01_V04",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C01_V06",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C01_V09",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C20_V20",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C20_V23",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C20_V25",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C20_V28",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C50_V50",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C50_V53",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C50_V55",
        1);
    tester.ExpectTotalCount(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithoutAccountConsistency.Binary_C50_V58",
        1);
  }

  // Validation group.
  {
    base::HistogramTester tester;
    validation_group.LogValidationMetricsUponOnProfileLoad(kGaiaId);

    // Metrics with account consistency record a deterministic bucket.
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V01",
        false, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V04",
        false, /*expected_bucket_count=*/1);
    // Thresholds larger than 0.05258f record `true`. Note that this includes
    // two additional metrics compared to control group tested earlier.
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V06",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C01_V09",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V20",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V23",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V25",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C20_V28",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V50",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V53",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V55",
        true, /*expected_bucket_count=*/1);
    tester.ExpectUniqueSample(
        "Sync.TrustedVaultAutoUpgrade.Validation.OnProfileLoadSampled."
        "WithAccountConsistency.Binary_C50_V58",
        true, /*expected_bucket_count=*/1);
  }
}

TEST(TrustedVaultSyntheticFieldTrialTest,
     ShouldNotLogValidationMetricsUponOnProfileLoadDueToSampling) {
  const int kCohort = 6;     // Not relevant in this test.
  const int kTypeIndex = 7;  // Not relevant in this test.
  const auto control_group =
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          kCohort, sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          kTypeIndex));
  const auto validation_group =
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(BuildTestProto(
          kCohort, sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION,
          kTypeIndex));

  // String chosen from previous tests.
  const std::string kGaiaId = "gaia_id_1";
  ASSERT_FALSE(TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                   ShouldSampleGaiaIdWithTenPercentProbabilityForTest(kGaiaId));

  base::HistogramTester tester;
  control_group.LogValidationMetricsUponOnProfileLoad(kGaiaId);
  validation_group.LogValidationMetricsUponOnProfileLoad(kGaiaId);

  // As per ASSERT_FALSE above, `kGaiaId` should be filtered out during sampling
  // and therefore no histograms should be recorded.
  EXPECT_THAT(tester.GetTotalCountsForPrefix("Sync."), IsEmpty());
}

}  // namespace

}  // namespace syncer
