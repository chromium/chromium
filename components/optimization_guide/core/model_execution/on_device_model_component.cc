// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {
namespace {

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

bool IsModelNeeded(const PrefService& local_state) {
  return WasAnOnDeviceFeatureRecentlyUsed(local_state) &&
         features::IsOnDeviceExecutionEnabled() && IsDeviceCapable(local_state);
}

}  // namespace

void OnDeviceModelComponentStateManager::UninstallComplete() {
  local_state_->ClearPref(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload);

  component_installer_registered_ = false;
}

void OnDeviceModelComponentStateManager::OnDeviceEligibleFeatureUsed() {
  local_state_->SetTime(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
      base::Time::Now());

  UpdateRegistration();
}

void OnDeviceModelComponentStateManager::DevicePerformanceClassChanged(
    OnDeviceModelPerformanceClass performance_class) {
  local_state_->SetInteger(prefs::localstate::kOnDevicePerformanceClass,
                           base::to_underlying(performance_class));

  UpdateRegistration();
}

void OnDeviceModelComponentStateManager::OnStartup() {
  // TODO(b/302327114): Add UMA.
  UpdateRegistration();
}

void OnDeviceModelComponentStateManager::UpdateRegistration() {
  // After the installer is registered, don't do anything until after a chrome
  // restart.
  if (component_installer_registered_) {
    return;
  }

  switch (GetRegistrationDecision()) {
    case OnDeviceRegistrationDecision::kDoNotInstall:
      break;
    case OnDeviceRegistrationDecision::kInstall:
      component_installer_registered_ = true;
      delegate_->RegisterInstaller(this);
      break;
    case OnDeviceRegistrationDecision::kUninstall:
      // Don't allow UpdateRegistration to do anything until after
      // UninstallComplete.
      component_installer_registered_ = true;
      delegate_->Uninstall(this);
      break;
  }
}

OnDeviceModelComponentStateManager::OnDeviceRegistrationDecision
OnDeviceModelComponentStateManager::GetRegistrationDecision() {
  if (IsModelNeeded(*local_state_)) {
    local_state_->SetTime(
        prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload,
        base::Time::Now());
    return OnDeviceRegistrationDecision::kInstall;
  }

  auto last_time_eligible = local_state_->GetTime(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload);
  if (last_time_eligible == base::Time::Min()) {
    return OnDeviceRegistrationDecision::kDoNotInstall;
  }

  const base::TimeDelta retention_time =
      features::GetOnDeviceModelRetentionTime();
  auto time_since_eligible = base::Time::Now() - last_time_eligible;
  if (time_since_eligible < retention_time &&
      time_since_eligible > -retention_time) {
    return OnDeviceRegistrationDecision::kInstall;
  }
  return OnDeviceRegistrationDecision::kUninstall;
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
  return state_.get();
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
       {kSpModelFile, kWeightsFile, kModelFile,
        kOnDeviceModelExecutionConfigFile}) {
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
  state_->version_ = version;
  for (auto& o : observers_) {
    o.StateChanged(state_.get());
  }
}

OnDeviceModelComponentState::OnDeviceModelComponentState() = default;
OnDeviceModelComponentState::~OnDeviceModelComponentState() = default;

}  // namespace optimization_guide
