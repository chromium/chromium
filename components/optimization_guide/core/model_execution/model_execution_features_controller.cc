// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/logging.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

ModelExecutionFeaturesController::ModelExecutionFeaturesController(
    PrefService* browser_context_profile_service,
    signin::IdentityManager* identity_manager)
    : browser_context_profile_service_(browser_context_profile_service),
      identity_manager_(identity_manager) {
  CHECK(browser_context_profile_service_);

  pref_change_registrar_.Init(browser_context_profile_service_);

  RecordFeatureSettingsAtStartup();
  InitializePrefListener();

  is_signed_in_ = identity_manager && identity_manager->HasPrimaryAccount(
                                          signin::ConsentLevel::kSignin);

  StartObservingAccountChanges(identity_manager);
}

ModelExecutionFeaturesController::~ModelExecutionFeaturesController() = default;

bool ModelExecutionFeaturesController::ShouldFeatureBeCurrentlyEnabledForUser(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsCurrentlyAValidUser()) {
    return false;
  }

  return features_enabled_at_startup_.find(static_cast<int>(feature)) !=
         features_enabled_at_startup_.end();
}

prefs::FeatureOptInState ModelExecutionFeaturesController::GetPrefState(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return prefs::FeatureOptInState::kNotInitialized;
    default:
      return static_cast<prefs::FeatureOptInState>(
          browser_context_profile_service_->GetInteger(
              prefs::GetSettingEnabledPrefName(feature)));
  }
  NOTREACHED();
  return prefs::FeatureOptInState::kNotInitialized;
}

bool ModelExecutionFeaturesController::IsCurrentlyAValidUser() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return is_signed_in_;
}

bool ModelExecutionFeaturesController::IsSettingVisible(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsCurrentlyAValidUser()) {
    return false;
  }

  // If the setting is currently enabled by user, then we should show the
  // setting to the user regardless of any other checks.
  if (ShouldFeatureBeCurrentlyEnabledForUser(feature)) {
    return true;
  }

  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      return false;
    default:
      return base::FeatureList::IsEnabled(
          *features::internal::GetFeatureToUseToCheckSettingsVisibility(
              feature));
  }
}

void ModelExecutionFeaturesController::AddObserver(
    SettingsEnabledObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  observers_.AddObserver(observer);
}

void ModelExecutionFeaturesController::RemoveObserver(
    SettingsEnabledObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  observers_.RemoveObserver(observer);
}

void ModelExecutionFeaturesController::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (identity_manager) {
    identity_manager->RemoveObserver(this);
  }
  identity_manager_ = nullptr;
}

void ModelExecutionFeaturesController::OnFeatureSettingPrefChanged(
    proto::ModelExecutionFeature feature) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsCurrentlyAValidUser()) {
    return;
  }

  for (SettingsEnabledObserver& obs : observers_) {
    if (obs.feature() != feature) {
      continue;
    }

    if (GetPrefState(feature) == prefs::FeatureOptInState::kEnabled) {
      obs.PrepareToEnableOnRestart();
    }
  }
}

void ModelExecutionFeaturesController::RecordFeatureSettingsAtStartup() {
  features_enabled_at_startup_.clear();
  for (int i = 0; i < proto::ModelExecutionFeature_ARRAYSIZE; ++i) {
    proto::ModelExecutionFeature feature = proto::ModelExecutionFeature(i);
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
        continue;
      default:
        if (GetPrefState(feature) == prefs::FeatureOptInState::kEnabled) {
          features_enabled_at_startup_.insert(static_cast<int>(feature));
        }
        continue;
    }
  }
}

void ModelExecutionFeaturesController::StartObservingAccountChanges(
    signin::IdentityManager* identity_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!identity_manager) {
    return;
  }

  identity_manager->AddObserver(this);
}

void ModelExecutionFeaturesController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool was_signed_in = is_signed_in_;

  is_signed_in_ = (identity_manager_ && identity_manager_->HasPrimaryAccount(
                                            signin::ConsentLevel::kSignin));

  if (was_signed_in == is_signed_in_) {
    return;
  }

  if (is_signed_in_) {
    return;
  }

  // Reset prefs to `kNotInitialized`.
  browser_context_profile_service_->SetInteger(
      optimization_guide::prefs::kModelExecutionMainToggleSettingState,
      static_cast<int>(prefs::FeatureOptInState::kNotInitialized));
  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
        continue;
      default:
        browser_context_profile_service_->SetInteger(
            optimization_guide::prefs::GetSettingEnabledPrefName(feature),
            static_cast<int>(prefs::FeatureOptInState::kNotInitialized));
    }
  }
}

void ModelExecutionFeaturesController::OnMainToggleSettingStatePrefChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsCurrentlyAValidUser()) {
    return;
  }

  bool is_now_enabled = (browser_context_profile_service_->GetInteger(
                             prefs::kModelExecutionMainToggleSettingState) ==
                         static_cast<int>(prefs::FeatureOptInState::kEnabled));

  prefs::FeatureOptInState feature_optin_state =
      is_now_enabled ? prefs::FeatureOptInState::kEnabled
                     : prefs::FeatureOptInState::kDisabled;

  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    if (feature ==
        proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED) {
      continue;
    }

    // If the main toggle has been switched from on to off, disable all the
    // features.
    if (feature_optin_state == prefs::FeatureOptInState::kDisabled) {
      browser_context_profile_service_->SetInteger(
          prefs::GetSettingEnabledPrefName(feature),
          static_cast<int>(feature_optin_state));
      continue;
    }
    // If the main toggle has been switched from off to on, then turn on
    // only the features that are actually visible to the user.
    if (IsSettingVisible(feature)) {
      browser_context_profile_service_->SetInteger(
          prefs::GetSettingEnabledPrefName(feature),
          static_cast<int>(feature_optin_state));
      continue;
    }
  }
}

void ModelExecutionFeaturesController::InitializePrefListener() {
  pref_change_registrar_.Add(
      optimization_guide::prefs::kModelExecutionMainToggleSettingState,
      base::BindRepeating(&ModelExecutionFeaturesController::
                              OnMainToggleSettingStatePrefChanged,
                          base::Unretained(this)));

  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    if (feature ==
        proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED) {
      continue;
    }

    pref_change_registrar_.Add(
        optimization_guide::prefs::GetSettingEnabledPrefName(feature),
        base::BindRepeating(
            &ModelExecutionFeaturesController::OnFeatureSettingPrefChanged,
            base::Unretained(this), feature));
  }
}

void ModelExecutionFeaturesController::SimulateBrowserRestartForTesting() {
  RecordFeatureSettingsAtStartup();
}

}  // namespace optimization_guide
