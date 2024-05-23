// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_
#define COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_

#include <iosfwd>
#include <string>
#include <string_view>

namespace sync_pb {
class TrustedVaultAutoUpgradeExperimentGroup;
}  // namespace sync_pb

namespace syncer {

inline constexpr char kTrustedVaultAutoUpgradeSyntheticFieldTrialName[] =
    "SyncTrustedVaultAutoUpgradeSyntheticTrial";

class TrustedVaultAutoUpgradeSyntheticFieldTrialGroup {
 public:
  // Special group name for the case where the existence of multiple browser
  // contexts (multiprofile) leads to the co-existence of two or more active
  // synthetic trial group names.
  static std::string GetMultiProfileConflictGroupName();

  // Constructs an instance from a protobuf. Returns an invalid instance,
  // detectable via `is_valid()`, if the input is invalid.
  static TrustedVaultAutoUpgradeSyntheticFieldTrialGroup FromProto(
      const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& group);

  // Constructs an invalid value.
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup();
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup(
      const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&);
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&&);
  ~TrustedVaultAutoUpgradeSyntheticFieldTrialGroup();

  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& operator=(
      const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&);
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& operator=(
      TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&&);

  bool is_valid() const { return !name_.empty(); }
  const std::string& name() const { return name_; }

  // Metric recording.
  void LogValidationMetricsUponOnProfileLoad(std::string_view gaia_id) const;

  // Exposed publicly for unit-testing.
  static float DeterministicFloatBetweenZeroAndOneFromGaiaIdForTest(
      std::string_view gaia_id,
      std::string_view salt);
  static bool ShouldSampleGaiaIdWithTenPercentProbabilityForTest(
      std::string_view gaia_id);

 private:
  void LogValidationMetrics(std::string_view gaia_id,
                            std::string_view short_metric_name) const;

  // Empty if `this` is invalid.
  std::string name_;
  // Set to true if this group has type VALIDATION.
  bool is_validation_group_type_ = false;
};

// gMock printer helper.
void PrintTo(const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group,
             std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_
