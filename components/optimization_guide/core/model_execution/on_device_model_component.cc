// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "services/on_device_model/public/cpp/cpu.h"
#include "services/on_device_model/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {
namespace {

// Delay to give consumers time to unload the model before it's deleted.
constexpr base::TimeDelta kUninstallDelay = base::Seconds(1);

void LogInstallCriteria(std::string_view event_name,
                        std::string_view criteria_name,
                        bool criteria_value) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria.",
           event_name, ".", criteria_name}),
      criteria_value);
}

void LogInstallCriteria(
    const OnDeviceModelComponentStateManager::RegistrationCriteria& criteria,
    std::string_view event_name,
    std::optional<int64_t> disk_space_gb = std::nullopt) {
  // Keep optimization/histograms.xml in sync with these criteria names.
  LogInstallCriteria(event_name, "DiskSpace",
                     criteria.is_disk_space_available());
  if (disk_space_gb && !criteria.is_disk_space_available()) {
    base::UmaHistogramCounts100(
        "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
        "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
        *disk_space_gb);
  }
  LogInstallCriteria(event_name, "DeviceCapability", criteria.device_capable);
  LogInstallCriteria(event_name, "FeatureUse",
                     criteria.on_device_feature_recently_used);
  LogInstallCriteria(event_name, "EnabledByFeature",
                     criteria.enabled_by_feature);
  LogInstallCriteria(event_name, "EnabledByEnterprisePolicy",
                     criteria.enabled_by_enterprise_policy);
  LogInstallCriteria(event_name, "All", criteria.should_install());
}

// Returns the best performance hint for this device based on the supported
// performance hints in the manifest. `prioritized_hints` is the
// list of performance hints in priority order, with highest priority first.
std::optional<proto::OnDeviceModelPerformanceHint>
GetBestPerformanceHintForDevice(
    const base::Value::List* manifest_performance_hints,
    const std::vector<proto::OnDeviceModelPerformanceHint>& prioritized_hints) {
  if (base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelForceCpuBackend)) {
    return proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU;
  }
  if (!manifest_performance_hints) {
    return std::nullopt;
  }

  absl::flat_hash_set<int> supported_hints;
  for (const auto& supported_performance_hint_val :
       *manifest_performance_hints) {
    std::optional<int> supported_performance_hint_int =
        supported_performance_hint_val.GetIfInt();
    if (supported_performance_hint_int) {
      supported_hints.insert(*supported_performance_hint_int);
    }
  }
  for (auto hint : prioritized_hints) {
    if (supported_hints.contains(base::to_underlying(hint))) {
      return hint;
    }
  }
  return std::nullopt;
}

// Reads the base model spec from the component manifest and potentially
// filters values to make it compatible with this device. `prioritized_hints`
// is the list of performance hints in priority order, with highest priority
// first.
std::optional<OnDeviceBaseModelSpec> GetOnDeviceBaseModelSpecFromManifest(
    const base::Value::Dict& manifest,
    const std::vector<proto::OnDeviceModelPerformanceHint>& prioritized_hints) {
  auto* model_spec = manifest.FindDict("BaseModelSpec");
  if (!model_spec) {
    return std::nullopt;
  }
  auto* name = model_spec->FindString("name");
  auto* version = model_spec->FindString("version");
  if (!name || !version) {
    return std::nullopt;
  }
  auto* supported_performance_hints =
      model_spec->FindList("supported_performance_hints");
  std::optional<proto::OnDeviceModelPerformanceHint> selected_performance_hint =
      GetBestPerformanceHintForDevice(supported_performance_hints,
                                      prioritized_hints);
  if (!selected_performance_hint) {
    return std::nullopt;
  }
  return OnDeviceBaseModelSpec(*name, *version, *selected_performance_hint);
}

base::Value::Dict MakeOverrideManifest() {
  auto hints =
      base::Value::List()
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
  return base::Value::Dict().Set(
      "BaseModelSpec",
      base::Value::Dict()
          .Set("name", "override")
          .Set("version", "override")
          .Set("supported_performance_hints", std::move(hints)));
}

}  // namespace

std::ostream& operator<<(std::ostream& out, OnDeviceModelStatus status) {
  switch (status) {
    case OnDeviceModelStatus::kReady:
      return out << "Ready";
    case OnDeviceModelStatus::kNotEligible:
      return out << "Not Eligible";
    case OnDeviceModelStatus::kInstallNotComplete:
      return out << "Install Not Complete";
    case OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason:
      return out << "Model Installer Not Registered For Unknown Reason";
    case OnDeviceModelStatus::kModelInstalledTooLate:
      return out << "Model Installed Too Late";
    case OnDeviceModelStatus::kNotReadyForUnknownReason:
      return out << "Not Ready For Unknown Reason";
    case OnDeviceModelStatus::kInsufficientDiskSpace:
      return out << "Insufficient Disk Space";
    case OnDeviceModelStatus::kNoOnDeviceFeatureUsed:
      return out << "No On-device Feature Used";
  }
}

OnDeviceBaseModelSpec::OnDeviceBaseModelSpec(
    const std::string& model_name,
    const std::string& model_version,
    proto::OnDeviceModelPerformanceHint selected_performance_hint)
    : model_name(model_name),
      model_version(model_version),
      selected_performance_hint(selected_performance_hint) {}
OnDeviceBaseModelSpec::~OnDeviceBaseModelSpec() = default;
OnDeviceBaseModelSpec::OnDeviceBaseModelSpec(const OnDeviceBaseModelSpec&) =
    default;

bool OnDeviceBaseModelSpec::operator==(
    const OnDeviceBaseModelSpec& other) const {
  return model_name == other.model_name &&
         model_version == other.model_version &&
         selected_performance_hint == other.selected_performance_hint;
}

OnDeviceModelComponentState::OnDeviceModelComponentState(
    base::FilePath install_dir,
    base::Version component_version,
    OnDeviceBaseModelSpec model_spec)
    : install_dir_(install_dir),
      component_version_(component_version),
      model_spec_(model_spec) {}
OnDeviceModelComponentState::OnDeviceModelComponentState(
    const OnDeviceModelComponentState&) = default;
OnDeviceModelComponentState::~OnDeviceModelComponentState() = default;

OnDeviceModelRegistrationAttributes::OnDeviceModelRegistrationAttributes(
    std::vector<proto::OnDeviceModelPerformanceHint> supported_hints)
    : supported_hints(std::move(supported_hints)) {}
OnDeviceModelRegistrationAttributes::OnDeviceModelRegistrationAttributes(
    const OnDeviceModelRegistrationAttributes&) = default;
OnDeviceModelRegistrationAttributes&
OnDeviceModelRegistrationAttributes::operator=(
    const OnDeviceModelRegistrationAttributes&) = default;
OnDeviceModelRegistrationAttributes::OnDeviceModelRegistrationAttributes(
    OnDeviceModelRegistrationAttributes&&) = default;
OnDeviceModelRegistrationAttributes&
OnDeviceModelRegistrationAttributes::operator=(
    OnDeviceModelRegistrationAttributes&&) = default;
OnDeviceModelRegistrationAttributes::~OnDeviceModelRegistrationAttributes() =
    default;

OnDeviceModelComponentStateManager::OnDeviceModelComponentStateManager(
    PrefService* local_state,
    base::SafeRef<PerformanceClassifier> performance_classifier,
    UsageTracker& usage_tracker,
    std::unique_ptr<Delegate> delegate)
    : local_state_(local_state),
      performance_classifier_(std::move(performance_classifier)),
      delegate_(std::move(delegate)),
      usage_tracker_(usage_tracker) {
  CHECK(local_state);  // Useful to catch poor test setup.
  usage_tracker_observation_.Observe(&usage_tracker);
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      base::BindRepeating(
          &OnDeviceModelComponentStateManager::
              OnGenAILocalFoundationalModelEnterprisePolicyChanged,
          weak_ptr_factory_.GetWeakPtr()));
  model_execution::prefs::PruneOldUsagePrefs(local_state_);
  performance_classifier_->ListenForPerformanceClassAvailable(base::BindOnce(
      &OnDeviceModelComponentStateManager::OnPerformanceClassAvailable,
      weak_ptr_factory_.GetWeakPtr()));
}

OnDeviceModelComponentStateManager::~OnDeviceModelComponentStateManager() =
    default;

// static
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

const OnDeviceModelComponentState*
OnDeviceModelComponentStateManager::GetState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!state_) {
    return nullptr;
  }

  // Even if the component is installed, we return nullptr if the model is not
  // 'allowed' at the moment.
  return registration_criteria_ && registration_criteria_->is_model_allowed()
             ? state_.get()
             : nullptr;
}

OnDeviceModelStatus
OnDeviceModelComponentStateManager::GetOnDeviceModelStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetState() != nullptr) {
    return OnDeviceModelStatus::kReady;
  }
  if (!registration_criteria_) {
    return OnDeviceModelStatus::kNotReadyForUnknownReason;
  }
  if (component_installer_registered_) {
    return OnDeviceModelStatus::kInstallNotComplete;
  }
  if (!registration_criteria_->is_model_allowed()) {
    return OnDeviceModelStatus::kNotEligible;
  }
  if (!registration_criteria_->is_disk_space_available()) {
    return OnDeviceModelStatus::kInsufficientDiskSpace;
  }
  if (!registration_criteria_->on_device_feature_recently_used) {
    return OnDeviceModelStatus::kNoOnDeviceFeatureUsed;
  }
  // This may happen before the first registration.
  return OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason;
}

void OnDeviceModelComponentStateManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void OnDeviceModelComponentStateManager::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

OnDeviceModelComponentStateManager::DebugState
OnDeviceModelComponentStateManager::GetDebugState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DebugState debug;
  debug.criteria_ = registration_criteria_.get();
  debug.disk_space_available_ = registration_criteria_
                                    ? registration_criteria_->disk_space_free
                                    : base::ByteCount(-1);
  debug.status_ = GetOnDeviceModelStatus();
  debug.has_override_ = !!switches::GetOnDeviceModelExecutionOverride();
  debug.state_ = state_.get();
  return debug;
}

void OnDeviceModelComponentStateManager::SetReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    const base::Value::Dict& manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_.reset();

  if (auto model_spec = GetOnDeviceBaseModelSpecFromManifest(
          manifest, performance_classifier_->GetPossibleHints())) {
    state_ = std::make_unique<OnDeviceModelComponentState>(install_dir, version,
                                                           *model_spec);
  }
  if (registration_criteria_ && registration_criteria_->is_model_allowed()) {
    NotifyStateChanged();
  }
}

void OnDeviceModelComponentStateManager::InstallerRegistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      state_ != nullptr);
}

void OnDeviceModelComponentStateManager::UninstallComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->ClearPref(model_execution::prefs::localstate::
                              kLastTimeEligibleForOnDeviceModelDownload);
  component_installer_registered_ = false;
}

void OnDeviceModelComponentStateManager::OnPerformanceClassAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::
    OnGenAILocalFoundationalModelEnterprisePolicyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::OnDeviceEligibleFeatureUsed(
    mojom::OnDeviceFeature feature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelStatusAtUseTime",
      GetOnDeviceModelStatus());

  if (registration_criteria_) {
    LogInstallCriteria(*registration_criteria_, "AtAttemptedUse");
  }

  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::BeginUpdateRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!performance_classifier_->IsPerformanceClassAvailable()) {
    // Still waiting for performance class.
    return;
  }
  if (auto model_path_override_switch =
          switches::GetOnDeviceModelExecutionOverride()) {
    // With an override, the model is always allowed.
    registration_criteria_ = std::make_unique<RegistrationCriteria>();
    registration_criteria_->device_capable = true;
    registration_criteria_->enabled_by_feature = true;
    registration_criteria_->enabled_by_enterprise_policy = true;
    if (!state_) {
      SetReady(base::Version("override"), *model_path_override_switch,
               MakeOverrideManifest());
    }
    return;
  }
  delegate_->GetFreeDiskSpace(
      delegate_->GetInstallDirectory(),
      base::BindOnce(
          &OnDeviceModelComponentStateManager::CompleteUpdateRegistration,
          GetWeakPtr()));
}

OnDeviceModelComponentStateManager::RegistrationCriteria
OnDeviceModelComponentStateManager::ComputeRegistrationCriteria(
    base::ByteCount disk_space_free_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegistrationCriteria result;
  result.disk_space_free = disk_space_free_bytes;
  result.device_capable = performance_classifier_->IsDeviceCapable();
  result.on_device_feature_recently_used =
      usage_tracker_->WasAnyOnDeviceEligibleFeatureRecentlyUsed();
  result.enabled_by_feature = features::IsOnDeviceExecutionEnabled();
  result.enabled_by_enterprise_policy =
      GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state_) ==
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;

  auto last_time_eligible =
      local_state_->GetTime(model_execution::prefs::localstate::
                                kLastTimeEligibleForOnDeviceModelDownload);

  result.is_already_installing = last_time_eligible != base::Time::Min();

  const base::TimeDelta retention_time =
      features::GetOnDeviceModelRetentionTime();
  auto time_since_eligible = base::Time::Now() - last_time_eligible;
  result.out_of_retention = time_since_eligible > retention_time ||
                            time_since_eligible < -retention_time;

  return result;
}

void OnDeviceModelComponentStateManager::CompleteUpdateRegistration(
    std::optional<base::ByteCount> disk_space_free) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/438265416): Handle failure to get free disk space.
  RegistrationCriteria criteria = ComputeRegistrationCriteria(
      disk_space_free.value_or(base::ByteCount(-1)));
  bool first_registration_attempt = !registration_criteria_;

  bool had_state = !!GetState();
  registration_criteria_ = std::make_unique<RegistrationCriteria>(criteria);
  if (!!GetState() != had_state) {
    NotifyStateChanged();
  }

  if (criteria.should_uninstall()) {
    // Don't allow UpdateRegistration to do anything until after
    // UninstallComplete.
    component_installer_registered_ = true;
    // Uninstall the component which will delete the model files, after a short
    // delay to give time for the consumers to unload the model.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceModelComponentStateManager::UninstallComponent,
                       GetWeakPtr()),
        kUninstallDelay);
  } else if (!component_installer_registered_ &&
             (criteria.should_install() || criteria.is_already_installing)) {
    component_installer_registered_ = true;
    delegate_->RegisterInstaller(
        GetWeakPtr(), OnDeviceModelRegistrationAttributes(
                          performance_classifier_->GetPossibleHints()));
  }

  if (criteria.should_install()) {
    local_state_->SetTime(model_execution::prefs::localstate::
                              kLastTimeEligibleForOnDeviceModelDownload,
                          base::Time::Now());
  }

  // Log metrics only for first registration attempt.
  if (first_registration_attempt) {
    LogInstallCriteria(criteria, "AtRegistration",
                       disk_space_free.value_or(base::ByteCount(-1)).InGiB());
  }
}

void OnDeviceModelComponentStateManager::UninstallComponent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->Uninstall(GetWeakPtr());
}

void OnDeviceModelComponentStateManager::NotifyStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& o : observers_) {
    o.StateChanged(GetState());
  }
}

}  // namespace optimization_guide
