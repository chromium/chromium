// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {
namespace {

const std::optional<OnDeviceBaseModelSpec> ReadBaseModelSpecFromManifest(
    const base::Value::Dict& manifest) {
  auto* model_spec = manifest.FindDict("BaseModelSpec");
  if (model_spec) {
    auto* name = model_spec->FindString("name");
    auto* version = model_spec->FindString("version");
    if (name && version) {
      return OnDeviceBaseModelSpec{*name, *version};
    }
  }
  return std::nullopt;
}

base::WeakPtr<OnDeviceModelComponentStateManager>& GetInstance() {
  static base::NoDestructor<base::WeakPtr<OnDeviceModelComponentStateManager>>
      state_manager_instance;
  return *state_manager_instance.get();
}

bool WasAnOnDeviceFeatureRecentlyUsed(const PrefService& local_state) {
  base::Time last_use = local_state.GetTime(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed);
  constexpr base::TimeDelta grace_period = base::Days(7);
  auto time_since_use = base::Time::Now() - last_use;
  // Note: Since we're storing a base::Time, we need to consider the possibility
  // of clock changes.
  return time_since_use < grace_period && time_since_use > -grace_period;
}

bool IsDeviceCapable(const PrefService& local_state) {
  int value =
      local_state.GetInteger(prefs::localstate::kOnDevicePerformanceClass);
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelPerformanceClass::kMaxValue)) {
    return false;
  }
  return features::IsPerformanceClassCompatibleWithOnDeviceModel(
      static_cast<OnDeviceModelPerformanceClass>(value));
}

void LogInstallCriteria(std::string_view event_name,
                        std::string_view criteria_name,
                        bool criteria_value) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria.",
           event_name, ".", criteria_name}),
      criteria_value);
}

}  // namespace

struct OnDeviceModelComponentStateManager::RegistrationCriteria {
  // Requirements for install. Please update `LogInstallCriteria()` when
  // updating this.
  bool disk_space_available = false;
  bool device_capable = false;
  bool on_device_feature_recently_used = false;
  bool enabled_by_feature = false;
  bool enabled_by_enterprise_policy = false;

  // Reasons to uninstall. TODO(b/302327114): Add UMA for uninstall reason.
  bool running_out_of_disk_space = false;
  bool out_of_retention = false;

  // Current state.

  // We've registered the installer in the past, and haven't uninstalled yet.
  // The component may or may not be ready.
  bool is_already_installing = false;

  bool is_model_allowed() const {
    return device_capable && enabled_by_feature && enabled_by_enterprise_policy;
  }

  bool should_install() const {
    if (should_uninstall()) {
      return false;
    }
    return (disk_space_available && is_model_allowed() &&
            on_device_feature_recently_used);
  }

  bool should_uninstall() const {
    return (is_already_installing &&
            (running_out_of_disk_space || out_of_retention));
  }
};

namespace {

void LogInstallCriteria(
    OnDeviceModelComponentStateManager::RegistrationCriteria& criteria,
    std::string_view event_name) {
  // Keep optimization/histograms.xml in sync with these criteria names.
  LogInstallCriteria(event_name, "DiskSpace", criteria.disk_space_available);
  LogInstallCriteria(event_name, "DeviceCapability", criteria.device_capable);
  LogInstallCriteria(event_name, "FeatureUse",
                     criteria.on_device_feature_recently_used);
  LogInstallCriteria(event_name, "EnabledByFeature",
                     criteria.enabled_by_feature);
  LogInstallCriteria(event_name, "EnabledByEnterprisePolicy",
                     criteria.enabled_by_enterprise_policy);
  LogInstallCriteria(event_name, "All", criteria.should_install());
}

}  // namespace

void OnDeviceModelComponentStateManager::UninstallComplete() {
  local_state_->ClearPref(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload);
  UpdateOnDeviceBaseModelSpecCache();
  component_installer_registered_ = false;
}

void OnDeviceModelComponentStateManager::OnDeviceEligibleFeatureUsed() {
  local_state_->SetTime(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
      base::Time::Now());

  OnDeviceModelStatus status;
  if (GetState() != nullptr) {
    status = OnDeviceModelStatus::kReady;
  } else if (!registration_criteria_) {
    status = OnDeviceModelStatus::kNotReadyForUnknownReason;
  } else {
    if (component_installer_registered_) {
      status = OnDeviceModelStatus::kInstallNotComplete;
    } else if (registration_criteria_->should_install()) {
      status =
          OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason;
    } else {
      status = OnDeviceModelStatus::kNotEligible;
    }
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelStatusAtUseTime", status);

  if (registration_criteria_) {
    LogInstallCriteria(*registration_criteria_, "AtAttemptedUse");
  }

  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::DevicePerformanceClassChanged(
    OnDeviceModelPerformanceClass performance_class) {
  local_state_->SetInteger(prefs::localstate::kOnDevicePerformanceClass,
                           base::to_underlying(performance_class));

  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::UpdateOnDeviceBaseModelSpecCache() {
  if (state_ && state_->GetBaseModelSpec()) {
    local_state_->SetString(prefs::localstate::kOnDeviceBaseModelVersion,
                            state_->GetBaseModelSpec().value().model_version);
    local_state_->SetString(prefs::localstate::kOnDeviceBaseModelName,
                            state_->GetBaseModelSpec().value().model_name);
  } else {
    local_state_->ClearPref(prefs::localstate::kOnDeviceBaseModelVersion);
    local_state_->ClearPref(prefs::localstate::kOnDeviceBaseModelName);
  }
}

const std::optional<OnDeviceBaseModelSpec>
OnDeviceModelComponentStateManager::GetCachedBaseModelSpec() {
  return OnDeviceBaseModelSpec{
      local_state_->GetString(prefs::localstate::kOnDeviceBaseModelName),
      local_state_->GetString(prefs::localstate::kOnDeviceBaseModelVersion)};
}

void OnDeviceModelComponentStateManager::OnStartup() {
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::InstallerRegistered() {
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      state_ != nullptr);
}

void OnDeviceModelComponentStateManager::BeginUpdateRegistration() {
  delegate_->GetFreeDiskSpace(
      delegate_->GetInstallDirectory(),
      base::BindOnce(
          &OnDeviceModelComponentStateManager::CompleteUpdateRegistration,
          GetWeakPtr()));
}

void OnDeviceModelComponentStateManager::CompleteUpdateRegistration(
    int64_t disk_space_free_bytes) {
  RegistrationCriteria criteria =
      GetRegistrationCriteria(disk_space_free_bytes);
  bool first_registration_attempt = !registration_criteria_;
  registration_criteria_ = std::make_unique<RegistrationCriteria>(criteria);

  if (criteria.should_install()) {
    local_state_->SetTime(
        prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload,
        base::Time::Now());
  }

  bool was_allowed = is_model_allowed_;
  is_model_allowed_ = criteria.is_model_allowed();
  if (state_ && was_allowed != is_model_allowed_) {
    NotifyStateChanged();
  }

  if (component_installer_registered_) {
    return;
  }

  if (criteria.should_uninstall()) {
    // Don't allow UpdateRegistration to do anything until after
    // UninstallComplete.
    component_installer_registered_ = true;
    delegate_->Uninstall(this);
  } else if (criteria.should_install() || criteria.is_already_installing) {
    component_installer_registered_ = true;
    delegate_->RegisterInstaller(this);
  }

  // Log metrics only for first registration attempt.
  if (!first_registration_attempt) {
    return;
  }
  LogInstallCriteria(criteria, "AtRegistration");
}

OnDeviceModelComponentStateManager::RegistrationCriteria
OnDeviceModelComponentStateManager::GetRegistrationCriteria(
    int64_t disk_space_free_bytes) {
  RegistrationCriteria result;
  result.running_out_of_disk_space = optimization_guide::features::
      IsFreeDiskSpaceTooLowForOnDeviceModelInstall(disk_space_free_bytes);
  result.disk_space_available = optimization_guide::features::
      IsFreeDiskSpaceSufficientForOnDeviceModelInstall(disk_space_free_bytes);
  result.device_capable = IsDeviceCapable(*local_state_);
  result.on_device_feature_recently_used =
      WasAnOnDeviceFeatureRecentlyUsed(*local_state_);
  result.enabled_by_feature = features::IsOnDeviceExecutionEnabled();
  result.enabled_by_enterprise_policy =
      GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state_) ==
      prefs::GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;

  auto last_time_eligible = local_state_->GetTime(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload);

  result.is_already_installing = last_time_eligible != base::Time::Min();

  const base::TimeDelta retention_time =
      features::GetOnDeviceModelRetentionTime();
  auto time_since_eligible = base::Time::Now() - last_time_eligible;
  result.out_of_retention = time_since_eligible > retention_time ||
                            time_since_eligible < -retention_time;

  return result;
}

OnDeviceModelComponentStateManager::OnDeviceModelComponentStateManager(
    PrefService* local_state,
    std::unique_ptr<Delegate> delegate)
    : local_state_(local_state), delegate_(std::move(delegate)) {
  CHECK(local_state);  // Useful to catch poor test setup.
}

OnDeviceModelComponentStateManager::~OnDeviceModelComponentStateManager() =
    default;

const OnDeviceModelComponentState*
OnDeviceModelComponentStateManager::GetState() {
  // Even if the component is installed, we return nullptr if the model is not
  // 'allowed' at the moment.
  return is_model_allowed_ ? state_.get() : nullptr;
}

scoped_refptr<OnDeviceModelComponentStateManager>
OnDeviceModelComponentStateManager::CreateOrGet(
    PrefService* local_state,
    std::unique_ptr<Delegate> delegate) {
  base::WeakPtr<OnDeviceModelComponentStateManager>& instance = GetInstance();
  if (!instance) {
    auto state_manager =
        base::WrapRefCounted(new OnDeviceModelComponentStateManager(
            local_state, std::move(delegate)));
    instance = state_manager->GetWeakPtr();
    return state_manager;
  }
  return scoped_refptr<OnDeviceModelComponentStateManager>(instance.get());
}

void OnDeviceModelComponentStateManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OnDeviceModelComponentStateManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
OnDeviceModelComponentStateManager*
OnDeviceModelComponentStateManager::GetInstanceForTesting() {
  return GetInstance().get();
}

bool OnDeviceModelComponentStateManager::VerifyInstallation(
    const base::FilePath& install_dir,
    const base::Value::Dict& manifest) {
  for (const base::FilePath::CharType* file_name :
       {kWeightsFile, kOnDeviceModelExecutionConfigFile}) {
    if (!base::PathExists(install_dir.Append(file_name))) {
      return false;
    }
  }
  return true;
}

void OnDeviceModelComponentStateManager::SetReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    const base::Value::Dict& manifest) {
  state_ = base::WrapUnique(new OnDeviceModelComponentState);
  state_->install_dir_ = install_dir;
  // This version refers to the component version specifically, not the model
  // version.
  state_->component_version_ = version;
  // Populate the model version and name from the manifest into the Chrome
  // pref cache. If not present, clears the state and cache.
  state_->model_spec_ = ReadBaseModelSpecFromManifest(manifest);
  UpdateOnDeviceBaseModelSpecCache();
  if (is_model_allowed_) {
    NotifyStateChanged();
  }
}

void OnDeviceModelComponentStateManager::NotifyStateChanged() {
  for (auto& o : observers_) {
    o.StateChanged(GetState());
  }
}

OnDeviceModelComponentState::OnDeviceModelComponentState() = default;
OnDeviceModelComponentState::~OnDeviceModelComponentState() = default;

}  // namespace optimization_guide
