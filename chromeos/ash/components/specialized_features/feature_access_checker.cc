// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include <utility>

#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"

namespace specialized_features {

using enum FeatureAccessFailure;

FeatureAccessConfig::FeatureAccessConfig() = default;
FeatureAccessConfig::~FeatureAccessConfig() = default;
FeatureAccessConfig::FeatureAccessConfig(const FeatureAccessConfig&) = default;
FeatureAccessConfig& FeatureAccessConfig::operator=(
    const FeatureAccessConfig&) = default;
FeatureAccessConfig& FeatureAccessConfig::operator=(FeatureAccessConfig&&) =
    default;

FeatureAccessChecker::FeatureAccessChecker(
    FeatureAccessConfig config,
    PrefService* prefs,
    signin::IdentityManager* identity_manager,
    VariationsServiceCallback variations_service_callback)
    : config_(config),
      prefs_(prefs),
      identity_manager_(identity_manager),
      variations_service_callback_(std::move(variations_service_callback)) {}

FeatureAccessChecker::FeatureAccessChecker() = default;

FeatureAccessChecker::~FeatureAccessChecker() = default;

FeatureAccessFailureSet FeatureAccessChecker::Check() const {
  FeatureAccessFailureSet failures;
  if (config_.disabled_in_kiosk_mode && chromeos::IsKioskSession()) {
    failures.Put(kDisabledInKioskModeCheckFailed);
  }

  if (config_.settings_toggle_pref.has_value()) {
    // if prefs service is not set, we should assume that the feature is not
    // enabled to be safe.
    if (prefs_ == nullptr ||
        !prefs_->GetBoolean(*config_.settings_toggle_pref)) {
      failures.Put(kDisabledInSettings);
    }
  }

  if (config_.consent_accepted_pref.has_value()) {
    if (prefs_ == nullptr ||
        !prefs_->GetBoolean(*config_.consent_accepted_pref)) {
      // if prefs service is not set, we should assume that the feature is not
      // enabled to be safe.
      failures.Put(kConsentNotAccepted);
    }
  }

  if (config_.feature_flag != nullptr &&
      !base::FeatureList::IsEnabled(*config_.feature_flag)) {
    failures.Put(kFeatureFlagDisabled);
  }

  if (config_.feature_management_flag != nullptr &&
      !base::FeatureList::IsEnabled(*config_.feature_management_flag)) {
    failures.Put(kFeatureManagementCheckFailed);
  }

  if (config_.secret_key.has_value() &&
      config_.secret_key->sha1_hashed_key_value !=
          base::SHA1HashString(
              base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                  config_.secret_key->flag))) {
    // if identity_manager_ is not set, we should assume that the feature is not
    // enabled to be safe.
    if (identity_manager_ == nullptr ||
        !config_.allow_google_accounts_skip_secret_key ||
        !gaia::IsGoogleInternalAccountEmail(
            identity_manager_
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                .email)) {
      failures.Put(kSecretKeyCheckFailed);
    }
  }

  if (!config_.capability_callback.is_null()) {
    if (identity_manager_ == nullptr ||
        (config_.capability_callback.Run(
             identity_manager_
                 ->FindExtendedAccountInfoByAccountId(
                     identity_manager_->GetPrimaryAccountId(
                         signin::ConsentLevel::kSignin))
                 .capabilities) != signin::Tribool::kTrue)) {
      failures.Put(kAccountCapabilitiesCheckFailed);
    }
  }

  if (!config_.country_codes.empty()) {
    variations::VariationsService* variations_service =
        variations_service_callback_.is_null()
            ? nullptr
            : variations_service_callback_.Run();
    if (variations_service == nullptr ||
        !base::Contains(config_.country_codes,
                        variations_service->GetLatestCountry())) {
      failures.Put(kCountryCheckFailed);
    }
  }

  return failures;
}

}  // namespace specialized_features
