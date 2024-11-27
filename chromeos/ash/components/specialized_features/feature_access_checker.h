// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"

namespace specialized_features {

// FeatureAccessFailures are the types of failures returned by a
// FeatureAccessChecker. They indicate which configured checks failed.
enum class COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES) FeatureAccessFailure {
  kMinValue = 0,
  kConsentNotAccepted = kMinValue,  // User consent not given
  kDisabledInSettings,              // Settings toggle is off
  kFeatureFlagDisabled,             // The feature flag disabled.
  kFeatureManagementCheckFailed,    // FeatureManagement flag was not enabled.
  kSecretKeyCheckFailed,            // Secret key required and was not valid.
  kMantaAccountCapabilitiesCheckFailed,  // Does not have account capabilities
                                         // for manta, which is a proxy to check
                                         // user requirements such as the user
                                         // inferred not to be a minor.
  kCountryCheckFailed,                   // Device's country is not authorised
                                         // to use this feature.
  kMaxValue = kCountryCheckFailed,
};

// EnumSet containing FeatureAccessFailures.
using FeatureAccessFailureSet = base::EnumSet<FeatureAccessFailure,
                                              FeatureAccessFailure::kMinValue,
                                              FeatureAccessFailure::kMaxValue>;

// Represents a secret key used to allow users to access a feature.
struct COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES) SecretKey {
  // Name of the flag.
  std::string_view flag;
  // The hashed value of the key.
  std::string_view sha1_hashed_key_value;
};

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
  // The main feature flag that is used to enable/disable the feature itself.
  // The FeatureAccessChecker::Check() verifies if its value is true and returns
  // kFeatureFlagDisabled if not.
  raw_ref<const base::Feature> feature_flag;
  // This is for the feature flag that is related to the ChromeOS feature
  // management system. The FeatureAccessChecker::Check() verifies if its value
  // is true and returns kFeatureManagementCheckFailed if not.
  raw_ref<const base::Feature> feature_management_flag;
  // Only these country codes are allowed. If empty, allows all country codes.
  base::raw_span<std::string_view> country_codes;
  // Special key used to guard users from accessing the feature.
  // FeatureAccessChecker::Check() only checks this special key if the optional
  // value is set since secret keys could be removed.
  // FeatureAccessChecker::Check() will return kSecretKeyCheckFailed if this
  // value is set and the secret key check fails.
  std::optional<SecretKey> secret_key;
  // Allow googlers to override the secret_key check.
  bool allow_google_accounts_skip_secret_key;
  // If set to true, will check if the account has manta account capabilities.
  // This is used as a proxy to check user capability requirements such as
  // whether the user is NOT inferred to be a minor.
  // FeatureAccessChecker::Check() will return
  // kMantaAccountCapabilitiesCheckFailed if this value is set and the secret
  // key check fails.
  bool requires_manta_account_capabilities;
};

// Creates a class to check different dependencies for specialized features.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessChecker {
 public:
  // The config determines which dependencies to check.
  // Buffers referred to by string_views and raw_span in `config` and instance
  // referred to by `prefs` and `identity_manager` should not be destroyed
  // before this class is destroyed.
  FeatureAccessChecker(FeatureAccessConfig config,
                       const PrefService& prefs,
                       const signin::IdentityManager& identity_manager,
                       const variations::VariationsService& variations_service);

  FeatureAccessChecker(const FeatureAccessChecker&) = delete;
  FeatureAccessChecker& operator=(const FeatureAccessChecker&) = delete;

  // Uses the set config and dependencies to check. Returns a list of errors.
  // For details of errors, please refer the comments of FeatureAccessConfig.
  FeatureAccessFailureSet Check();

 private:
  FeatureAccessConfig config_;
  raw_ref<const PrefService> prefs_;
  raw_ref<const signin::IdentityManager> identity_manager_;
  raw_ref<const variations::VariationsService> variations_service_;
};

}  // namespace specialized_features

#endif  // CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
