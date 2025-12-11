// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"

namespace specialized_features {

// FeatureAccessFailures are the types of failures returned by a
// FeatureAccessChecker. They indicate which configured checks failed.
enum class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessFailure {
      kMinValue = 0,
      kConsentNotAccepted = kMinValue,  // User consent not given
      kDisabledInSettings,              // Settings toggle is off
      kFeatureFlagDisabled,             // The feature flag disabled.
      kFeatureManagementCheckFailed,  // FeatureManagement flag was not enabled.
      kSecretKeyCheckFailed,          // Secret key required and was not valid.
      kAccountCapabilitiesCheckFailed,  // Does not have required account
                                        // capabilities, which is a proxy to
                                        // check user requirements such as the
                                        // user inferred not to be a minor.
      kCountryCheckFailed,              // Device's country is not authorised
                                        // to use this feature.
      kDisabledInKioskModeCheckFailed,  // In Kiosk mode.
      kMaxValue = kDisabledInKioskModeCheckFailed,
    };

// EnumSet containing FeatureAccessFailures.
using FeatureAccessFailureSet = base::EnumSet<FeatureAccessFailure,
                                              FeatureAccessFailure::kMinValue,
                                              FeatureAccessFailure::kMaxValue>;

// Represents a secret key used to allow users to access a feature.
struct COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES) SecretKey {
  // Name of the flag.
  std::string flag;
  // The hashed value of the key.
  std::string sha1_hashed_key_value;
};

// This configures the FeatureAccessChecker for different types of common
// specialized feature checks.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessConfig {
  FeatureAccessConfig();
  ~FeatureAccessConfig();
  FeatureAccessConfig(const FeatureAccessConfig&);
  FeatureAccessConfig& operator=(const FeatureAccessConfig&);
  FeatureAccessConfig& operator=(FeatureAccessConfig&&);

  // Disable for kiosk mode.
  bool disabled_in_kiosk_mode = false;

  // The preference that determines whether the feature is enabled via settings.
  // The FeatureAccessChecker::Check() verifies if its value is true in the
  // given prefs, and returns kDisabledInSettings if not.
  std::optional<std::string> settings_toggle_pref;
  // The preference that deterimines whether the features legal consent
  // disclaimer has been accepted. The FeatureAccessChecker::Check() verifies
  // if its value is true in the given prefs, and returns kConsentNotAccepted if
  // not.
  std::optional<std::string> consent_accepted_pref;
  // The main feature flag that is used to enable/disable the feature itself.
  // The FeatureAccessChecker::Check() verifies if its value is true and returns
  // kFeatureFlagDisabled if not.
  // Since feature flags are not copyable, we have to use a pointer.
  // This is safe for feature flags declared by `BASE_FEATURE` and
  // `BASE_DECLARE_FEATURE` since they are created-module level and never
  // destroyed.
  raw_ptr<const base::Feature> feature_flag = nullptr;
  // This is for the feature flag that is related to the ChromeOS feature
  // management system. The FeatureAccessChecker::Check() verifies if its value
  // is true and returns kFeatureManagementCheckFailed if not.
  // Since feature flags are not copyable, we have to use a pointer.
  // This is safe for feature flags declared by `BASE_FEATURE` and
  // `BASE_DECLARE_FEATURE` since they are created-module level and never
  // destroyed.
  raw_ptr<const base::Feature> feature_management_flag = nullptr;
  // Only these country codes are allowed. If empty, allows all country codes.
  std::vector<std::string> country_codes;
  // Special key used to guard users from accessing the feature.
  // FeatureAccessChecker::Check() only checks this special key if the optional
  // value is set since secret keys could be removed.
  // FeatureAccessChecker::Check() will return kSecretKeyCheckFailed if this
  // value is set and the secret key check fails.
  std::optional<SecretKey> secret_key;
  // Allow googlers to override the secret_key check.
  bool allow_google_accounts_skip_secret_key = false;
  // If not null, it will check if the account has the required account
  // capabilities. This is used as a proxy to check user capability requirements
  // such as whether the user is NOT inferred to be a minor.
  // FeatureAccessChecker::Check() will return
  // kAccountCapabilitiesCheckFailed if the return value is not
  // signin::Tribool::kTrue.
  // Example usage:
  // config.capability_callback =
  //     base::BindRepeating([](AccountCapabilities capabilities) {
  //       return capabilities.can_use_manta_service();
  //     });
  base::RepeatingCallback<signin::Tribool(AccountCapabilities capabilities)>
      capability_callback;
};

// Creates a class to check different dependencies for specialized features.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
    FeatureAccessChecker {
 public:
  using VariationsServiceCallback =
      base::RepeatingCallback<variations::VariationsService*()>;

  // The config determines which dependencies to check.
  // The following objects should not be destroyed before this class is
  // destroyed:
  // - prefs
  // - identity_manager
  // If any checks require the above dependency and it is destroy, the check
  // will return a failure.
  FeatureAccessChecker(FeatureAccessConfig config,
                       PrefService* prefs,
                       signin::IdentityManager* identity_manager,
                       VariationsServiceCallback variations_service_callback);

  FeatureAccessChecker(const FeatureAccessChecker&) = delete;
  FeatureAccessChecker& operator=(const FeatureAccessChecker&) = delete;
  virtual ~FeatureAccessChecker();

  // Uses the set config and dependencies to check. Returns a list of errors.
  // For details of errors, please refer the comments of FeatureAccessConfig.
  virtual FeatureAccessFailureSet Check() const;

 protected:
  // Constructor for mock subclass.
  FeatureAccessChecker();

 private:
  FeatureAccessConfig config_;
  // The following raw_ptrs are not owned by this class.
  raw_ptr<const PrefService> prefs_;
  raw_ptr<const signin::IdentityManager> identity_manager_;
  VariationsServiceCallback variations_service_callback_;
};

}  // namespace specialized_features

#endif  // CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEATURE_ACCESS_CHECKER_H_
