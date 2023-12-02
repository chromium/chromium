// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

enum class SettingsVisibilityResult {
  kUnknown = 0,
  // Not visible because user is invalid.
  kNotVisibleInvalidUser = 1,
  // Visible because feature is already enabled.
  kVisibleFeatureAlreadyEnabled = 2,
  // Not visible because field trial is disabled.
  kNotVisibleFieldTrialDisabled = 3,
  // Visible because field trial is enabled.
  kVisibleFieldTrialEnabled = 4,
  // Updates should match with FeaturesSettingsVisibilityResult enum in
  // enums.xml.
  kMaxValue = kVisibleFieldTrialEnabled
};

// Util class for recording the construction and validation of Settings
// Visibility histogram.
class ScopedSettingsVisibilityResultHistogramRecorder {
 public:
  explicit ScopedSettingsVisibilityResultHistogramRecorder() = default;

  ~ScopedSettingsVisibilityResultHistogramRecorder() {
    CHECK(is_valid_);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.SettingsVisibilityResult.",
             GetStringNameForModelExecutionFeature(feature_)}),
        result_);
  }

  void SetValid() { is_valid_ = true; }

  void SetResult(proto::ModelExecutionFeature feature,
                 SettingsVisibilityResult result) {
    is_valid_ = true;
    feature_ = feature;
    result_ = result;
  }

 private:
  bool is_valid_ = false;
  proto::ModelExecutionFeature feature_;
  SettingsVisibilityResult result_;
};

enum class FeatureCurrentlyEnabledResult {
  kUnknown = 0,
  // Not enabled because user is invalid.
  kNotEnabledInvalidUser = 1,
  // Returned result as enabled because feature was enabled at startup.
  kEnabledAtStartup = 2,
  // Returned result as not enabled because feature was not enabled at startup.
  kNotEnabledAtStartup = 3,
  // Updates should match with FeatureCurrentlyEnabledResult enum in enums.xml.
  kMaxValue = kNotEnabledAtStartup
};

// Util class for recording the construction and validation of Settings
// Visibility histogram.
class ScopedFeatureCurrentlyEnabledHistogramRecorder {
 public:
  explicit ScopedFeatureCurrentlyEnabledHistogramRecorder() = default;

  ~ScopedFeatureCurrentlyEnabledHistogramRecorder() {
    CHECK(is_valid_);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.FeatureCurrentlyEnabledResult.",
             GetStringNameForModelExecutionFeature(feature_)}),
        result_);
  }

  void SetResult(proto::ModelExecutionFeature feature,
                 FeatureCurrentlyEnabledResult result) {
    is_valid_ = true;
    feature_ = feature;
    result_ = result;
  }

 private:
  bool is_valid_ = false;
  proto::ModelExecutionFeature feature_;
  FeatureCurrentlyEnabledResult result_;
};

}  // namespace

ModelExecutionFeaturesController::ModelExecutionFeaturesController(
    PrefService* browser_context_profile_service,
    signin::IdentityManager* identity_manager)
    : browser_context_profile_service_(browser_context_profile_service),
      identity_manager_(identity_manager),
      features_allowed_for_unsigned_user_(
          features::internal::GetAllowedFeaturesForUnsignedUser()) {
  CHECK(browser_context_profile_service_);

  pref_change_registrar_.Init(browser_context_profile_service_);

  InitializeFeatureSettings();
  InitializePrefListener();

  is_signed_in_ = identity_manager && identity_manager->HasPrimaryAccount(
                                          signin::ConsentLevel::kSignin);

  StartObservingAccountChanges();
}

ModelExecutionFeaturesController::~ModelExecutionFeaturesController() = default;

bool ModelExecutionFeaturesController::ShouldFeatureBeCurrentlyEnabledForUser(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedFeatureCurrentlyEnabledHistogramRecorder metrics_recorder;

  if (!IsCurrentlyAValidUser(feature)) {
    metrics_recorder.SetResult(
        feature, FeatureCurrentlyEnabledResult::kNotEnabledInvalidUser);
    return false;
  }

  bool result = features_enabled_at_startup_.find(static_cast<int>(feature)) !=
                features_enabled_at_startup_.end();

  metrics_recorder.SetResult(
      feature, result ? FeatureCurrentlyEnabledResult::kEnabledAtStartup
                      : FeatureCurrentlyEnabledResult::kNotEnabledAtStartup);

  return result;
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

bool ModelExecutionFeaturesController::IsCurrentlyAValidUser(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (feature ==
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED) {
    return false;
  }

  // Sign-in check.
  if (!is_signed_in_ &&
      !base::Contains(features_allowed_for_unsigned_user_, feature)) {
    return false;
  }

  return true;
}

bool ModelExecutionFeaturesController::IsSettingVisible(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedSettingsVisibilityResultHistogramRecorder metrics_recorder;

  if (!IsCurrentlyAValidUser(feature)) {
    metrics_recorder.SetResult(
        feature, SettingsVisibilityResult::kNotVisibleInvalidUser);
    return false;
  }

  // If the setting is currently enabled by user, then we should show the
  // setting to the user regardless of any other checks.
  if (ShouldFeatureBeCurrentlyEnabledForUser(feature)) {
    metrics_recorder.SetResult(
        feature, SettingsVisibilityResult::kVisibleFeatureAlreadyEnabled);
    return true;
  }

  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      metrics_recorder.SetValid();
      return false;
    default:
      bool result = base::FeatureList::IsEnabled(
          *features::internal::GetFeatureToUseToCheckSettingsVisibility(
              feature));
      SettingsVisibilityResult visibility_result =
          result ? SettingsVisibilityResult::kVisibleFieldTrialEnabled
                 : SettingsVisibilityResult::kNotVisibleFieldTrialDisabled;
      metrics_recorder.SetResult(feature, visibility_result);
      return result;
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

  if (!IsCurrentlyAValidUser(feature)) {
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

void ModelExecutionFeaturesController::InitializeFeatureSettings() {
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

void ModelExecutionFeaturesController::StartObservingAccountChanges() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  identity_manager_observation_.Observe(identity_manager_);
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
  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
        continue;
      default:
        if (!IsCurrentlyAValidUser(feature)) {
          browser_context_profile_service_->SetInteger(
              optimization_guide::prefs::GetSettingEnabledPrefName(feature),
              static_cast<int>(prefs::FeatureOptInState::kNotInitialized));
        }
    }
  }
}

void ModelExecutionFeaturesController::OnMainToggleSettingStatePrefChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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
  InitializeFeatureSettings();
}

}  // namespace optimization_guide
