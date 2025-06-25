// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_synthetic_field_trial.h"

#include <ostream>
#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

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
