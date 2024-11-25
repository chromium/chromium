// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "components/prefs/pref_service.h"

namespace specialized_features {

using enum FeatureAccessFailure;

FeatureAccessChecker::FeatureAccessChecker(FeatureAccessConfig config,
                                           const PrefService& prefs)
    : config_(config), prefs_(prefs) {}

FeatureAccessFailureSet FeatureAccessChecker::Check() {
  FeatureAccessFailureSet failures;

  if (!prefs_->GetBoolean(config_.settings_toggle_pref)) {
    failures.Put(kDisabledInSettings);
  }

  if (!prefs_->GetBoolean(config_.consent_accepted_pref)) {
    failures.Put(kConsentNotAccepted);
  };

  if (!base::FeatureList::IsEnabled(*config_.feature_flag)) {
    failures.Put(kFeatureFlagDisabled);
  }

  if (!base::FeatureList::IsEnabled(*config_.feature_management_flag)) {
    failures.Put(kFeatureManagementCheckFailed);
  }

  if (config_.secret_key.has_value() &&
      config_.secret_key->sha1_hashed_key_value !=
          base::SHA1HashString(
              base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                  config_.secret_key->flag))) {
    failures.Put(kSecretKeyCheckFailed);
  }

  return failures;
}

}  // namespace specialized_features
