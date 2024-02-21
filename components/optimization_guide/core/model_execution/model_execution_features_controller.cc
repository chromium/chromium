// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace optimization_guide {

namespace {

bool ShouldCheckSettingForFeature(proto::ModelExecutionFeature feature) {
  return feature != proto::MODEL_EXECUTION_FEATURE_UNSPECIFIED &&
         feature != proto::MODEL_EXECUTION_FEATURE_TEST;
}

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

  void SetResult(
      proto::ModelExecutionFeature feature,
      ModelExecutionFeaturesController::SettingsVisibilityResult result) {
    is_valid_ = true;
    feature_ = feature;
    result_ = result;
  }

 private:
  bool is_valid_ = false;
  proto::ModelExecutionFeature feature_;
  ModelExecutionFeaturesController::SettingsVisibilityResult result_;
};

enum class FeatureCurrentlyEnabledResult {
  kUnknown = 0,
  // Not enabled because user is not signed-in.
  kNotEnabledUnsignedUser = 1,
  // Returned result as enabled because feature was enabled at startup.
  kEnabledAtStartup = 2,
  // Returned result as not enabled because feature was not enabled at startup.
  kNotEnabledAtStartup = 3,
  // Returned result as not enabled because feature was disabled by enterprise
  // policy.
  kNotEnabledEnterprisePolicy = 4,
  // Returned result as not enabled because model execution capability was
  // disabled for the user account.
  kNotEnabledModelExecutionCapability = 5,
  // Updates should match with FeatureCurrentlyEnabledResult enum in enums.xml.
  kMaxValue = kNotEnabledModelExecutionCapability
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

// Returns whether the model execution capability is enabled. Use this whenever
// the `AccountInfo` is available which has more recent data, instead of
// querying via the `IdentityManager` that could be having stale information.
bool CanUseModelExecutionFeaturesFromAccountInfo(
    const AccountInfo account_info) {
  if (base::FeatureList::IsEnabled(
          features::internal::kModelExecutionCapabilityDisable)) {
    // Disable the capability check and allow all model execution features.
    return true;
  }
  return account_info.capabilities.can_use_model_execution_features() !=
         signin::Tribool::kFalse;
}

bool CanUseModelExecutionFeatures(signin::IdentityManager* identity_manager) {
  if (base::FeatureList::IsEnabled(
          features::internal::kModelExecutionCapabilityDisable)) {
    // Disable the capability check and allow all model execution features.
    return true;
  }
  if (!identity_manager) {
    return false;
  }
  const auto account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }
  return CanUseModelExecutionFeaturesFromAccountInfo(
      identity_manager->FindExtendedAccountInfoByAccountId(account_id));
}

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
  if (is_signed_in_) {
    can_use_model_execution_features_ =
        CanUseModelExecutionFeatures(identity_manager);
  }

  StartObservingAccountChanges();
}

ModelExecutionFeaturesController::~ModelExecutionFeaturesController() = default;

bool ModelExecutionFeaturesController::ShouldFeatureBeCurrentlyEnabledForUser(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedFeatureCurrentlyEnabledHistogramRecorder metrics_recorder;
  bool is_enabled = GetPrefState(feature) == prefs::FeatureOptInState::kEnabled;
  metrics_recorder.SetResult(
      feature, is_enabled
                   ? FeatureCurrentlyEnabledResult::kEnabledAtStartup
                   : FeatureCurrentlyEnabledResult::kNotEnabledAtStartup);
  return is_enabled;
}

bool ModelExecutionFeaturesController::
    ShouldFeatureBeCurrentlyAllowedForLogging(
        proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!ShouldFeatureBeCurrentlyEnabledForUser(feature)) {
    return false;
  }
  return GetEnterprisePolicyValue(feature) ==
         model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow;
}

prefs::FeatureOptInState ModelExecutionFeaturesController::GetPrefState(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!ShouldCheckSettingForFeature(feature)) {
    NOTREACHED();
    return prefs::FeatureOptInState::kNotInitialized;
  }

  return static_cast<prefs::FeatureOptInState>(
      browser_context_profile_service_->GetInteger(
          prefs::GetSettingEnabledPrefName(feature)));
}

ModelExecutionFeaturesController::UserValidityResult
ModelExecutionFeaturesController::GetCurrentUserValidityResult(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(ShouldCheckSettingForFeature(feature));

  // Sign-in check.
  if (!is_signed_in_ &&
      !base::Contains(features_allowed_for_unsigned_user_, feature)) {
    return ModelExecutionFeaturesController::UserValidityResult::
        kInvalidUnsignedUser;
  }

  // Check user account is allowed to use model execution, when signed-in.
  if (is_signed_in_ && !can_use_model_execution_features_) {
    return ModelExecutionFeaturesController::UserValidityResult::
        kInvalidModelExecutionCapability;
  }

  if (GetEnterprisePolicyValue(feature) ==
      model_execution::prefs::ModelExecutionEnterprisePolicyValue::kDisable) {
    return ModelExecutionFeaturesController::UserValidityResult::
        kInvalidEnterprisePolicy;
  }

  return ModelExecutionFeaturesController::UserValidityResult::kValid;
}

bool ModelExecutionFeaturesController::IsSettingVisible(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedSettingsVisibilityResultHistogramRecorder metrics_recorder;

  switch (GetCurrentUserValidityResult(feature)) {
    case ModelExecutionFeaturesController::UserValidityResult::
        kInvalidUnsignedUser:
      metrics_recorder.SetResult(
          feature, SettingsVisibilityResult::kNotVisibleUnsignedUser);
      return false;
    case ModelExecutionFeaturesController::UserValidityResult::
        kInvalidEnterprisePolicy:
      metrics_recorder.SetResult(
          feature, SettingsVisibilityResult::kNotVisibleEnterprisePolicy);
      return false;
    case ModelExecutionFeaturesController::UserValidityResult::
        kInvalidModelExecutionCapability:
      metrics_recorder.SetResult(
          feature,
          SettingsVisibilityResult::kNotVisibleModelExecutionCapability);
      return false;
    case ModelExecutionFeaturesController::UserValidityResult::kValid:
      break;
  }

  // If the setting is currently enabled by user, then we should show the
  // setting to the user regardless of any other checks.
  if (ShouldFeatureBeCurrentlyEnabledForUser(feature)) {
    metrics_recorder.SetResult(
        feature, SettingsVisibilityResult::kVisibleFeatureAlreadyEnabled);
    return true;
  }

  if (!ShouldCheckSettingForFeature(feature)) {
    metrics_recorder.SetValid();
    return false;
  }

  bool result = base::FeatureList::IsEnabled(
      *features::internal::GetFeatureToUseToCheckSettingsVisibility(feature));
  SettingsVisibilityResult visibility_result =
      result ? SettingsVisibilityResult::kVisibleFieldTrialEnabled
             : SettingsVisibilityResult::kNotVisibleFieldTrialDisabled;
  metrics_recorder.SetResult(feature, visibility_result);
  return result;
}

model_execution::prefs::ModelExecutionEnterprisePolicyValue
ModelExecutionFeaturesController::GetEnterprisePolicyValue(
    proto::ModelExecutionFeature feature) const {
  const char* enterprise_policy_pref =
      model_execution::prefs::GetEnterprisePolicyPrefName(feature);
  CHECK(enterprise_policy_pref);
  return static_cast<
      model_execution::prefs::ModelExecutionEnterprisePolicyValue>(
      browser_context_profile_service_->GetInteger(enterprise_policy_pref));
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

  auto pref_value = GetPrefState(feature);
  bool is_enabled = ShouldFeatureBeCurrentlyEnabledForUser(feature);

  // When the feature is enabled, check the user is valid to enable the
  // feature.
  CHECK(!is_enabled ||
            GetCurrentUserValidityResult(feature) ==
                ModelExecutionFeaturesController::UserValidityResult::kValid,
        base::NotFatalUntil::M125);

  if (pref_value != prefs::FeatureOptInState::kNotInitialized) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange.",
             GetStringNameForModelExecutionFeature(feature)}),
        is_enabled);
  }
  for (SettingsEnabledObserver& obs : observers_) {
    if (obs.feature() != feature) {
      continue;
    }
    obs.OnChangeInFeatureCurrentlyEnabledState(is_enabled);
  }
}

void ModelExecutionFeaturesController::OnFeatureEnterprisePolicyPrefChanged(
    proto::ModelExecutionFeature feature) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // When enterprise policy changes from allowed to disallowed, the feature
  // settings prefs need to be cleared. This in turn triggers
  // `OnFeatureSettingPrefChanged` to notify the observers of settings change,
  // when the pref is reset.
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::InitializeFeatureSettings() {
  for (int i = 0; i < proto::ModelExecutionFeature_ARRAYSIZE; ++i) {
    proto::ModelExecutionFeature feature = proto::ModelExecutionFeature(i);
    if (!ShouldCheckSettingForFeature(feature)) {
      continue;
    }

    base::UmaHistogramBoolean(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.",
             GetStringNameForModelExecutionFeature(feature)}),
        ShouldFeatureBeCurrentlyEnabledForUser(feature));
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

  if (!is_signed_in_) {
    can_use_model_execution_features_ = false;
    ResetInvalidFeaturePrefs();
    return;
  }
  can_use_model_execution_features_ =
      CanUseModelExecutionFeatures(identity_manager_);
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_signed_in_) {
    can_use_model_execution_features_ = false;
    ResetInvalidFeaturePrefs();
    return;
  }
  can_use_model_execution_features_ =
      CanUseModelExecutionFeaturesFromAccountInfo(info);
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::ResetInvalidFeaturePrefs() {
  bool main_toggle_enabled =
      (browser_context_profile_service_->GetInteger(
           prefs::kModelExecutionMainToggleSettingState) ==
       static_cast<int>(prefs::FeatureOptInState::kEnabled));

  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    if (!ShouldCheckSettingForFeature(feature)) {
      continue;
    }
    auto pref_state = GetPrefState(feature);

    // When the main toggle is enabled, and the feature pref was never disabled
    // by the user, it can be enabled, if it is visible in settings.
    if (main_toggle_enabled && IsSettingVisible(feature) &&
        (pref_state == prefs::FeatureOptInState::kNotInitialized)) {
      browser_context_profile_service_->SetInteger(
          prefs::GetSettingEnabledPrefName(feature),
          static_cast<int>(prefs::FeatureOptInState::kEnabled));
    }

    // Reset prefs that were enabled to `kNotInitialized` when the conditions
    // disallow the feature.
    if (pref_state == prefs::FeatureOptInState::kEnabled &&
        GetCurrentUserValidityResult(feature) !=
            ModelExecutionFeaturesController::UserValidityResult::kValid) {
      browser_context_profile_service_->SetInteger(
          optimization_guide::prefs::GetSettingEnabledPrefName(feature),
          static_cast<int>(prefs::FeatureOptInState::kNotInitialized));
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
    if (!ShouldCheckSettingForFeature(feature)) {
      continue;
    }
    // Do not change the pref for invisible features.
    if (!IsSettingVisible(feature)) {
      continue;
    }
    // Set the feature pref the same state as the main toggle.
    browser_context_profile_service_->SetInteger(
        prefs::GetSettingEnabledPrefName(feature),
        static_cast<int>(feature_optin_state));
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
    if (!ShouldCheckSettingForFeature(feature)) {
      continue;
    }

    pref_change_registrar_.Add(
        optimization_guide::prefs::GetSettingEnabledPrefName(feature),
        base::BindRepeating(
            &ModelExecutionFeaturesController::OnFeatureSettingPrefChanged,
            base::Unretained(this), feature));
    pref_change_registrar_.Add(
        model_execution::prefs::GetEnterprisePolicyPrefName(feature),
        base::BindRepeating(&ModelExecutionFeaturesController::
                                OnFeatureEnterprisePolicyPrefChanged,
                            base::Unretained(this), feature));
  }
}

}  // namespace optimization_guide
