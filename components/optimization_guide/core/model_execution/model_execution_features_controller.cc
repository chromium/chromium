// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/feature_registry/settings_ui_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/tflite/buildflags.h"

namespace optimization_guide {

namespace {

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
      UserVisibleFeatureKey feature,
      ModelExecutionFeaturesController::SettingsVisibilityResult result) {
    is_valid_ = true;
    feature_ = feature;
    result_ = result;
  }

 private:
  bool is_valid_ = false;
  UserVisibleFeatureKey feature_;
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
  // Returned result as enabled because the feature has graduated from
  // experimental AI settings.
  kEnabledByGraduation = 6,
  // Updates should match with FeatureCurrentlyEnabledResult enum in enums.xml.
  kMaxValue = kEnabledByGraduation
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

  void SetResult(UserVisibleFeatureKey feature,
                 FeatureCurrentlyEnabledResult result) {
    is_valid_ = true;
    feature_ = feature;
    result_ = result;
  }

 private:
  bool is_valid_ = false;
  UserVisibleFeatureKey feature_;
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
    signin::IdentityManager* identity_manager,
    PrefService* local_state,
    DogfoodStatus dogfood_status)
    : browser_context_profile_service_(browser_context_profile_service),
      identity_manager_(identity_manager),
      local_state_(local_state),
      features_allowed_for_unsigned_user_(
          features::internal::GetAllowedFeaturesForUnsignedUser()),
      dogfood_status_(dogfood_status) {
  CHECK(browser_context_profile_service_);

  pref_change_registrar_.Init(browser_context_profile_service_);

  InitializeFeatureSettings();
  InitializePrefListener();

  is_signed_in_ = identity_manager && identity_manager->HasPrimaryAccount(
                                          signin::ConsentLevel::kSignin);
  if (is_signed_in_) {
    account_allows_model_execution_features_ =
        CanUseModelExecutionFeatures(identity_manager);
  }

  StartObservingAccountChanges();
}

ModelExecutionFeaturesController::~ModelExecutionFeaturesController() = default;

bool ModelExecutionFeaturesController::ShouldFeatureBeCurrentlyEnabledForUser(
    UserVisibleFeatureKey feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedFeatureCurrentlyEnabledHistogramRecorder metrics_recorder;

  if (features::internal::IsGraduatedFeature(feature)) {
    UserValidityResult user_validity = GetCurrentUserValidityResult(feature);
    // TODO(b/328523679): also report the FeatureCurrentlyEnabledResult values
    // below for non-graduated features.
    FeatureCurrentlyEnabledResult fcer;
    switch (user_validity) {
      case UserValidityResult::kValid:
        fcer = FeatureCurrentlyEnabledResult::kEnabledByGraduation;
        break;
      case UserValidityResult::kInvalidUnsignedUser:
        fcer = FeatureCurrentlyEnabledResult::kNotEnabledUnsignedUser;
        break;
      case UserValidityResult::kInvalidEnterprisePolicy:
        fcer = FeatureCurrentlyEnabledResult::kNotEnabledEnterprisePolicy;
        break;
      case UserValidityResult::kInvalidModelExecutionCapability:
        fcer =
            FeatureCurrentlyEnabledResult::kNotEnabledModelExecutionCapability;
        break;
    };
    metrics_recorder.SetResult(feature, fcer);
    return user_validity == UserValidityResult::kValid;
  }

  bool is_enabled = GetPrefState(feature) == prefs::FeatureOptInState::kEnabled;
  metrics_recorder.SetResult(
      feature, is_enabled
                   ? FeatureCurrentlyEnabledResult::kEnabledAtStartup
                   : FeatureCurrentlyEnabledResult::kNotEnabledAtStartup);
  return is_enabled;
}

bool ModelExecutionFeaturesController::
    ShouldFeatureAllowModelExecutionForSignedInUser(
        optimization_guide::UserVisibleFeatureKey feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Check common things like enterprise policy, etc. first. The basic feature
  // with `allow_unsigned_user` true is more permissive. This check adds the
  // additional requirement that user is signed in and allowed execution.
  // This is needed by kHistorySearch feature to gate answer generation
  // on account details while allowing all users (including unsigned users)
  // access to basic embeddings search functionality.
  return ShouldFeatureBeCurrentlyEnabledForUser(feature) &&
         PerformSigninChecks() == UserValidityResult::kValid;
}

bool ModelExecutionFeaturesController::
    ShouldFeatureBeCurrentlyAllowedForLogging(
        const MqlsFeatureMetadata* metadata) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<UserVisibleFeatureKey> feature_key =
      metadata->user_visible_feature_key();
  if (feature_key && !ShouldFeatureBeCurrentlyEnabledForUser(*feature_key)) {
    return false;
  }

  // For dogfood users only, allow the relevant chrome://flags option to
  // override the default enterprise policy.
  bool has_logging_force_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableModelQualityDogfoodLogging);
  if (dogfood_status_ == DogfoodStatus::DOGFOOD && has_logging_force_enabled) {
    return true;
  }

  return metadata->enterprise_policy().GetValue(
             browser_context_profile_service_) ==
         model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow;
}

prefs::FeatureOptInState ModelExecutionFeaturesController::GetPrefState(
    UserVisibleFeatureKey feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return static_cast<prefs::FeatureOptInState>(
      browser_context_profile_service_->GetInteger(
          prefs::GetSettingEnabledPrefName(feature)));
}

ModelExecutionFeaturesController::UserValidityResult
ModelExecutionFeaturesController::GetCurrentUserValidityResult(
    UserVisibleFeatureKey feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool require_account =
      !base::Contains(features_allowed_for_unsigned_user_, feature);

  if (require_account) {
    UserValidityResult signin_checks_result = PerformSigninChecks();
    if (signin_checks_result != UserValidityResult::kValid) {
      return signin_checks_result;
    }
  }

  const SettingsUiMetadata* metadata =
      SettingsUiRegistry::GetInstance().GetFeature(feature);
  CHECK(metadata);
  if (metadata->enterprise_policy().GetValue(
          browser_context_profile_service_) ==
      model_execution::prefs::ModelExecutionEnterprisePolicyValue::kDisable) {
    return ModelExecutionFeaturesController::UserValidityResult::
        kInvalidEnterprisePolicy;
  }

  return ModelExecutionFeaturesController::UserValidityResult::kValid;
}

ModelExecutionFeaturesController::UserValidityResult
ModelExecutionFeaturesController::PerformSigninChecks() const {
  // Sign-in check.
  if (!is_signed_in_) {
    return UserValidityResult::kInvalidUnsignedUser;
  }

  // Check user account is allowed to use model execution, when signed-in.
  if (!account_allows_model_execution_features_) {
    return UserValidityResult::kInvalidModelExecutionCapability;
  }

  return UserValidityResult::kValid;
}

ModelExecutionFeaturesController::SettingsVisibilityResult
ModelExecutionFeaturesController::ShouldHideHistorySearch() const {
#if !BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  return SettingsVisibilityResult::kNotVisibleHardwareUnsupported;
#else
  // Component updates policy check.
  if (!local_state_->GetBoolean(::prefs::kComponentUpdatesEnabled)) {
    return SettingsVisibilityResult::kNotVisibleEnterprisePolicy;
  }

  // Performance class check.
  std::string allowed_classes_string =
      features::internal::kPerformanceClassListForHistorySearch.Get();
  if (allowed_classes_string == "*" || allowed_classes_string.empty()) {
    return SettingsVisibilityResult::kUnknown;
  }

  int perf_class = local_state_->GetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);
  std::vector<std::string_view> allowed_classes_list = base::SplitStringPiece(
      allowed_classes_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::Contains(allowed_classes_list, base::ToString(perf_class))
             ? SettingsVisibilityResult::kUnknown
             : SettingsVisibilityResult::kNotVisibleHardwareUnsupported;
#endif
}

bool ModelExecutionFeaturesController::IsSettingVisible(
    UserVisibleFeatureKey feature) const {
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

  // Graduated feature should never be visible in settings.
  if (features::internal::IsGraduatedFeature(feature)) {
    metrics_recorder.SetResult(
        feature, SettingsVisibilityResult::kNotVisibleGraduatedFeature);
    return false;
  }

  // Check feature-specific requirements.
  if (feature == UserVisibleFeatureKey::kHistorySearch) {
    SettingsVisibilityResult result = ShouldHideHistorySearch();
    if (result != SettingsVisibilityResult::kUnknown) {
      metrics_recorder.SetResult(feature, result);
      return false;
    }
  }

  // If the setting is currently enabled by user, then we should show the
  // setting to the user regardless of any other checks.
  if (ShouldFeatureBeCurrentlyEnabledForUser(feature)) {
    metrics_recorder.SetResult(
        feature, SettingsVisibilityResult::kVisibleFeatureAlreadyEnabled);
    return true;
  }

  bool result = base::FeatureList::IsEnabled(
      *features::internal::GetFeatureToUseToCheckSettingsVisibility(feature));
  SettingsVisibilityResult visibility_result =
      result ? SettingsVisibilityResult::kVisibleFieldTrialEnabled
             : SettingsVisibilityResult::kNotVisibleFieldTrialDisabled;
  metrics_recorder.SetResult(feature, visibility_result);
  return result;
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
    UserVisibleFeatureKey feature) {
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
    UserVisibleFeatureKey feature) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // When enterprise policy changes from allowed to disallowed, the feature
  // settings prefs need to be cleared. This in turn triggers
  // `OnFeatureSettingPrefChanged` to notify the observers of settings change,
  // when the pref is reset.
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::InitializeFeatureSettings() {
  for (auto key : kAllUserVisibleFeatureKeys) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.",
             GetStringNameForModelExecutionFeature(key)}),
        ShouldFeatureBeCurrentlyEnabledForUser(key));
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
    account_allows_model_execution_features_ = false;
    ResetInvalidFeaturePrefs();
    return;
  }
  account_allows_model_execution_features_ =
      CanUseModelExecutionFeatures(identity_manager_);
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_signed_in_) {
    account_allows_model_execution_features_ = false;
    ResetInvalidFeaturePrefs();
    return;
  }
  account_allows_model_execution_features_ =
      CanUseModelExecutionFeaturesFromAccountInfo(info);
  ResetInvalidFeaturePrefs();
}

void ModelExecutionFeaturesController::ResetInvalidFeaturePrefs() {
  bool main_toggle_enabled =
      (browser_context_profile_service_->GetInteger(
           prefs::kModelExecutionMainToggleSettingState) ==
       static_cast<int>(prefs::FeatureOptInState::kEnabled));

  for (auto feature : kAllUserVisibleFeatureKeys) {
    auto pref_state = GetPrefState(feature);

    // When the main toggle is enabled, and the feature pref was never disabled
    // by the user, it can be enabled, if it is visible in settings, and allowed
    // for automatic turning on.
    if (main_toggle_enabled && IsSettingVisible(feature) &&
        features::internal::ShouldEnableFeatureWhenMainToggleOn(feature) &&
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

void ModelExecutionFeaturesController::AllowUnsignedUserForTesting(
    UserVisibleFeatureKey feature) {
  features_allowed_for_unsigned_user_.insert(feature);
}

void ModelExecutionFeaturesController::OnMainToggleSettingStatePrefChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool is_now_enabled = (browser_context_profile_service_->GetInteger(
                             prefs::kModelExecutionMainToggleSettingState) ==
                         static_cast<int>(prefs::FeatureOptInState::kEnabled));

  prefs::FeatureOptInState feature_optin_state =
      is_now_enabled ? prefs::FeatureOptInState::kEnabled
                     : prefs::FeatureOptInState::kDisabled;

  for (auto feature : kAllUserVisibleFeatureKeys) {
    // Do not change the pref for invisible features.
    if (!IsSettingVisible(feature)) {
      continue;
    }
    if (!features::internal::ShouldEnableFeatureWhenMainToggleOn(feature)) {
      // Do not change features that don't want to be changed with main toggle.
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

  SettingsUiRegistry& registry = SettingsUiRegistry::GetInstance();
  for (auto feature : kAllUserVisibleFeatureKeys) {
    pref_change_registrar_.Add(
        optimization_guide::prefs::GetSettingEnabledPrefName(feature),
        base::BindRepeating(
            &ModelExecutionFeaturesController::OnFeatureSettingPrefChanged,
            base::Unretained(this), feature));
    const SettingsUiMetadata* metadata = registry.GetFeature(feature);
    CHECK(metadata);
    pref_change_registrar_.Add(
        metadata->enterprise_policy().name(),
        base::BindRepeating(&ModelExecutionFeaturesController::
                                OnFeatureEnterprisePolicyPrefChanged,
                            base::Unretained(this), feature));
  }
}

}  // namespace optimization_guide
