// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "crypto/secure_hash.h"

namespace syncer {
namespace {

// Arbitrary and generious limit for the cohort ID.
constexpr int kMaxCohortId = 100;

// Arbitrary and generous limit for the group type index.
constexpr int kMaxGroupTypeIndex = 50;

const char* GetGroupTypeName(
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type) {
  switch (type) {
    case sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TYPE_UNSPECIFIED:
      return "";
    case sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TREATMENT:
      return "Treatment";
    case sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL:
      return "Control";
    case sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION:
      return "Validation";
  }
  NOTREACHED();
}

std::string GetGroupName(
    int cohort,
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type,
    int type_index) {
  if (cohort <= 0 || cohort > kMaxCohortId) {
    // Invalid cohort ID.
    return std::string();
  }

  if (type_index < 0 || type_index > kMaxGroupTypeIndex) {
    // Invalid type index.
    return std::string();
  }

  const char* type_str = GetGroupTypeName(type);
  if (!*type_str) {
    // Invalid type.
    return std::string();
  }

  std::string type_index_str;
  if (type_index > 0) {
    type_index_str = base::StringPrintf("%d", type_index);
  }

  return base::StringPrintf("Cohort%d_%s%s", cohort, type_str,
                            type_index_str.c_str());
}

// Returns a random-like float in range [0, 1) that is computed
// deterministically from `gaia_id` and `salt`.
float DeterministicFloatBetweenZeroAndOneFromGaiaId(std::string_view gaia_id,
                                                    std::string_view salt) {
  CHECK(!gaia_id.empty());

  const std::string_view kSuffix = "TrustedVaultAutoUpgrade";
  uint64_t value = 0;

  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  sha256->Update(gaia_id.data(), gaia_id.length());
  sha256->Update(salt.data(), salt.length());
  sha256->Update(kSuffix.data(), kSuffix.length());
  sha256->Finish(&value, sizeof(value));

  const int kResolution = 100000;
  return 1.0f * (value % kResolution) / kResolution;
}

bool ShouldSampleGaiaIdWithTenPercentProbability(std::string_view gaia_id) {
  const float kGaiaIdSamplingFactor = 0.1f;
  const std::string_view kSaltForUserSampling = "UserSampling";

  return DeterministicFloatBetweenZeroAndOneFromGaiaId(
             gaia_id, kSaltForUserSampling) < kGaiaIdSamplingFactor;
}

}  // namespace

// static
std::string TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    GetMultiProfileConflictGroupName() {
  return "MultiProfileConflict";
}

// static
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
    const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& proto) {
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup instance;
  instance.name_ =
      GetGroupName(proto.cohort(), proto.type(), proto.type_index());
  instance.is_validation_group_type_ =
      (proto.type() ==
       sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION);
  return instance;
}

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    TrustedVaultAutoUpgradeSyntheticFieldTrialGroup() = default;

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    TrustedVaultAutoUpgradeSyntheticFieldTrialGroup(
        const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&) = default;

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    TrustedVaultAutoUpgradeSyntheticFieldTrialGroup(
        TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&&) = default;

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    ~TrustedVaultAutoUpgradeSyntheticFieldTrialGroup() = default;

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::operator=(
    const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&) = default;

TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::operator=(
    TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&&) = default;

void TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    LogValidationMetricsUponOnProfileLoad(std::string_view gaia_id) const {
  CHECK(is_valid());

  if (gaia_id.empty()) {
    // Shouldn't be reachable, but in edge cases (preferences corrupted) it
    // could be.
    return;
  }

  // This metrics gets logged for 10% gaia IDs to artifially reduce the volume
  // of data.
  if (!ShouldSampleGaiaIdWithTenPercentProbability(gaia_id)) {
    return;
  }

  LogValidationMetrics(gaia_id, "OnProfileLoadSampled");
}

void TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::LogValidationMetrics(
    std::string_view gaia_id,
    std::string_view short_metric_name) const {
  const struct {
    const std::string_view name_suffix;
    float probability_default;
    float probability_validation;
  } boolean_metrics[] = {
      {"Binary_C01_V01", 0.01f, 0.01f}, {"Binary_C01_V04", 0.01f, 0.04f},
      {"Binary_C01_V06", 0.01f, 0.06f}, {"Binary_C01_V09", 0.01f, 0.09f},
      {"Binary_C20_V20", 0.20f, 0.20f}, {"Binary_C20_V23", 0.20f, 0.23f},
      {"Binary_C20_V25", 0.20f, 0.25f}, {"Binary_C20_V28", 0.20f, 0.28f},
      {"Binary_C50_V50", 0.50f, 0.50f}, {"Binary_C50_V53", 0.50f, 0.53f},
      {"Binary_C50_V55", 0.50f, 0.55f}, {"Binary_C50_V58", 0.50f, 0.58f},
  };

  for (bool use_account_consistency : {false, true}) {
    const std::string_view metric_infix = use_account_consistency
                                              ? ".WithAccountConsistency."
                                              : ".WithoutAccountConsistency.";
    const float value_between_zero_and_one =
        use_account_consistency ? DeterministicFloatBetweenZeroAndOneFromGaiaId(
                                      gaia_id, /*salt=*/short_metric_name)
                                : base::RandFloat();

    CHECK_GE(value_between_zero_and_one, 0.0f);
    CHECK_LT(value_between_zero_and_one, 1.0f);

    for (const auto& metric : boolean_metrics) {
      const std::string full_metric_name =
          base::StrCat({"Sync.TrustedVaultAutoUpgrade.Validation.",
                        short_metric_name, metric_infix, metric.name_suffix});
      const float probability = is_validation_group_type_
                                    ? metric.probability_validation
                                    : metric.probability_default;
      const bool success = value_between_zero_and_one < probability;
      base::UmaHistogramBoolean(full_metric_name, success);
    }
  }
}

// static
float TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    DeterministicFloatBetweenZeroAndOneFromGaiaIdForTest(
        std::string_view gaia_id,
        std::string_view salt) {
  return DeterministicFloatBetweenZeroAndOneFromGaiaId(gaia_id, salt);
}

// static
bool TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
    ShouldSampleGaiaIdWithTenPercentProbabilityForTest(
        std::string_view gaia_id) {
  return ShouldSampleGaiaIdWithTenPercentProbability(gaia_id);
}

void PrintTo(const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group,
             std::ostream* os) {
  if (group.is_valid()) {
    *os << group.name();
  } else {
    *os << "<invalid-group>";
  }
}

}  // namespace syncer
