// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/recovery_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/system/sys_info.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Note: The trial name *must* match the study name in the server-side
// configuration.
const char kCryptohomeRecoveryTrialName[] = "CryptohomeRecovery";
const char kCryptohomeRecoveryGroupName[] = "CryptohomeRecovery_FirstRunTrial";

}  // namespace

void CreateFallbackFieldTrialForRecovery(bool is_stable_channel,
                                         base::FeatureList* feature_list) {
  // Don't create the trial if either feature is enabled in chrome://flags. This
  // condition is to avoid having multiple registered trials overriding the same
  // feature.
  if (feature_list->IsFeatureOverridden(features::kCryptohomeRecovery.name)) {
    return;
  }

  // Recovery is controlled by a flag which is off by default. The local field
  // trial ensures that recovery is enabled on some channels but can later be
  // disabled by Finch when appropriate.
  if (is_stable_channel) {
      // On the stable channel we don't enable the field trial and therefore
      // keep using the default (off) value of the flag.
      return;
  }

  scoped_refptr<base::FieldTrial> trial(base::FieldTrialList::CreateFieldTrial(
      kCryptohomeRecoveryTrialName, kCryptohomeRecoveryGroupName));

  if (trial.get()) {
    feature_list->RegisterFieldTrialOverride(
        features::kCryptohomeRecovery.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  }
}

bool IsRecoveryOptInAvailable(bool is_managed) {
  return features::IsCryptohomeRecoveryEnabled() && !is_managed;
}

bool GetRecoveryDefaultState(bool is_managed, PrefService* prefs) {
  if (is_managed) {
    return prefs->GetBoolean(ash::prefs::kRecoveryFactorBehavior);
  }

  // For non-managed users the default state is "disabled".
  return false;
}

}  // namespace ash
