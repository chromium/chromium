// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"

namespace specialized_features {

// FeatureAccessFailures are the types of failures returned by a
// FeatureAccessChecker. They indicate which configured checks failed.
enum class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessFailure {
      kMinValue = 0,
      kConsentNotAccepted = kMinValue,  // User consent not given
      kDisabledInSettings,              // Settings toggle is off
      kMaxValue = kDisabledInSettings,
    };

// EnumSet containing FeatureAccessFailures.
using FeatureAccessFailureSet = base::EnumSet<FeatureAccessFailure,
                                              FeatureAccessFailure::kMinValue,
                                              FeatureAccessFailure::kMaxValue>;

// This configures the FeatureAccessChecker for different types of common
// specialized feature checks.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessConfig {
  // The preference that determines whether the feature is enabled via settings.
  // The FeatureAccessChecker::Check() verifies if its value is true in the
  // given prefs, and returns kDisabledInSettings if not.
  std::string_view settings_toggle_pref;
  // The preference that deterimines whether the features legal consent
  // disclaimer has been accepted. The FeatureAccessChecker::Check() verifies
  // if its value is true in the given prefs, and returns kConsentNotAccepted if
  // not.
  std::string_view consent_accepted_pref;
};

// Creates a class to check different dependencies for specialized features.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessChecker {
 public:
  // The config determines which dependencies to check.
  // Buffers referred to by string_views in `config` and instance referred to by
  // `prefs` should not be destroyed before this class is destroyed.
  FeatureAccessChecker(FeatureAccessConfig config, const PrefService& prefs);

  FeatureAccessChecker(const FeatureAccessChecker&) = delete;
  FeatureAccessChecker& operator=(const FeatureAccessChecker&) = delete;

  // Uses the set config and dependencies to check. Returns a list of errors.
  // For details of errors, please refer the comments of FeatureAccessConfig.
  FeatureAccessFailureSet Check();

 private:
  FeatureAccessConfig config_;
  raw_ref<const PrefService> prefs_;
};

}  // namespace specialized_features

#endif  // CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
