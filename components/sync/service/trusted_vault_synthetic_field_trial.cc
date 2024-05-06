// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include <ostream>
#include <string>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace syncer {
namespace {

// Arbitrary and generious limit for the cohort ID.
constexpr int kMaxCohortId = 100;

std::string GetGroupName(
    sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::AutoUpgradeExperimentGroup
        group,
    int cohort_id) {
  if (cohort_id <= 0 || cohort_id > kMaxCohortId) {
    // Invalid cohort ID.
    return std::string();
  }

  switch (group) {
    case sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::
        AUTO_UPGRADE_EXPERIMENT_GROUP_UNSPECIFIED:
      return std::string();
    case sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::TREATMENT:
      return base::StringPrintf("Treatment_%d", cohort_id);
    case sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::CONTROL:
      return base::StringPrintf("Control_%d", cohort_id);
    case sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::VALIDATION:
      return base::StringPrintf("Validation_%d", cohort_id);
  }

  NOTREACHED_NORETURN();
}

}  // namespace

// static
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup
TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
    sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::AutoUpgradeExperimentGroup
        group,
    int cohort_id) {
  TrustedVaultAutoUpgradeSyntheticFieldTrialGroup instance;
  instance.name_ = GetGroupName(group, cohort_id);
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

void PrintTo(const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group,
             std::ostream* os) {
  if (group.is_valid()) {
    *os << group.name();
  } else {
    *os << "<invalid-group>";
  }
}

}  // namespace syncer
