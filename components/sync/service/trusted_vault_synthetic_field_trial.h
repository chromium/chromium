// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_
#define COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_

#include <iosfwd>
#include <string>

#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

class TrustedVaultAutoUpgradeSyntheticFieldTrialGroup {
 public:
  // Constructs an instance from a protobuf. Returns an invalid instance,
  // detectable via `is_valid()`, if the input is invalid.
  static TrustedVaultAutoUpgradeSyntheticFieldTrialGroup FromProto(
      sync_pb::NigoriSpecifics::AutoUpgradeDebugInfo::AutoUpgradeExperimentGroup
          group,
      int cohort_id);

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

 private:
  // Empty if `this` is invalid.
  std::string name_;
};

// gMock printer helper.
void PrintTo(const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group,
             std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_SYNTHETIC_FIELD_TRIAL_H_
