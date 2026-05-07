// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "net/base/network_change_notifier.h"
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
  LogInstallCriteria(event_name, "EnabledByUserSetting",
                     criteria.enabled_by_user_setting);
  LogInstallCriteria(event_name, "All",
                     criteria.get_install_mode().has_value());
  if (criteria.get_install_mode().has_value() &&
      !criteria.is_already_installing) {
    LogInstallCriteria(
        "InitialInstall", "IsBackground",
        criteria.get_install_mode() == ModelInstallMode::kRegisterOnly);
  }
}

// Returns the best performance hint for this device based on the supported
// performance hints in the manifest. `prioritized_hints` is the
// list of performance hints in priority order, with highest priority first.
std::optional<proto::OnDeviceModelPerformanceHint>
GetBestPerformanceHintForDevice(
    const base::ListValue* manifest_performance_hints,
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
    if (supported_hints.contains(std::to_underlying(hint))) {
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
    const base::DictValue& manifest,
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

base::DictValue MakeOverrideManifest() {
  auto hints =
      base::ListValue()
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
  return base::DictValue().Set(
      "BaseModelSpec",
      base::DictValue()
          .Set("name", "override")
          .Set("version", "override")
          .Set("supported_performance_hints", std::move(hints)));
}

// `BaseModel` is deliberately made to be the same as an enum class of the same
// name in
// components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.cc.
// This is to allow logging of new model installations regardless of which model
// management scheme is used.
enum class BaseModel {
  kUnknown = 0,
  kXxs = 1,
  kXs = 2,
  kV2Nano = 3,
  kV3Nano = 4,
  kMaxValue = kV3Nano,
};

BaseModel ConvertModelNameToEnum(const std::string& model_name) {
  if (model_name == "v3Nano") {
    return BaseModel::kV3Nano;
  } else if (model_name == "v2Nano") {
    return BaseModel::kV2Nano;
  } else if (model_name == "XS") {
    return BaseModel::kXs;
  } else if (model_name == "XXS") {
    return BaseModel::kXxs;
  } else {
    return BaseModel::kUnknown;
  }
}

std::string GetUmaModelNameFromState(OnDeviceModelComponentState* state) {
  if (!state || state->GetBaseModelSpec().model_name == "v3Nano") {
    return "V3Nano";
  } else {
    return "Unknown";
  }
}

bool WasOnDeviceModelRecentlyUsed(UsageTracker* usage_tracker,
                                  OnDeviceModelType model_type) {
  return std::ranges::any_of(
      OnDeviceFeatureSet::All(), [&](mojom::OnDeviceFeature feature) {
        return GetOnDeviceModelType(feature) == model_type &&
               usage_tracker->WasUseCaseRecentlyUsed(ToUseCaseName(feature));
      });
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
    base::flat_set<proto::OnDeviceModelPerformanceHint> supported_hints)
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

OnDeviceModelComponentStateManager::RegistrationCriteria::
    RegistrationCriteria() = default;
OnDeviceModelComponentStateManager::RegistrationCriteria::
    ~RegistrationCriteria() = default;
OnDeviceModelComponentStateManager::RegistrationCriteria::RegistrationCriteria(
    const RegistrationCriteria&) = default;
OnDeviceModelComponentStateManager::RegistrationCriteria&
OnDeviceModelComponentStateManager::RegistrationCriteria::operator=(
    const RegistrationCriteria&) = default;
OnDeviceModelComponentStateManager::RegistrationCriteria::RegistrationCriteria(
    RegistrationCriteria&&) = default;
OnDeviceModelComponentStateManager::RegistrationCriteria&
OnDeviceModelComponentStateManager::RegistrationCriteria::operator=(
    RegistrationCriteria&&) = default;

OnDeviceModelComponentStateManager::OnDeviceModelComponentStateManager(
    PrefService* local_state,
    base::SafeRef<PerformanceClassifier> performance_classifier,
    UsageTracker& usage_tracker,
    std::unique_ptr<Delegate> delegate,
    OnDeviceModelType model_type)
    : local_state_(local_state),
      performance_classifier_(std::move(performance_classifier)),
      delegate_(std::move(delegate)),
      usage_tracker_(usage_tracker),
      model_type_(model_type) {
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
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      base::BindRepeating(&OnDeviceModelComponentStateManager::
                              OnGenAILocalFoundationalModelUserSettingChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  model_execution::prefs::PruneOldUsagePrefs(local_state_);
  performance_classifier_->ListenForPerformanceClassAvailable(base::BindOnce(
      &OnDeviceModelComponentStateManager::OnPerformanceClassAvailable,
      weak_ptr_factory_.GetWeakPtr()));
  base::UmaHistogramBoolean(
      "OptimizationGuide.OnDeviceModel.OnDeviceModelComponentInstantiated",
      true);
}

OnDeviceModelComponentStateManager::~OnDeviceModelComponentStateManager() =
    default;

// static
bool OnDeviceModelComponentStateManager::VerifyInstallation(
    const base::FilePath& install_dir,
    const base::DictValue& manifest) {
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

void OnDeviceModelComponentStateManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  observer->StateChanged(GetOnDeviceModelState());
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

std::vector<mojom::BrokerPropertyInfoPtr>
OnDeviceModelComponentStateManager::GetBrokerProperties() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<mojom::BrokerPropertyInfoPtr> props;
  if (registration_criteria_) {
    props.push_back(mojom::BrokerPropertyInfo::New(
        "Feature recently used",
        base::ToString(
            registration_criteria_->on_device_feature_recently_used)));
    props.push_back(mojom::BrokerPropertyInfo::New(
        "Enabled by feature flag",
        base::ToString(registration_criteria_->enabled_by_feature)));
    props.push_back(mojom::BrokerPropertyInfo::New(
        "Enabled by enterprise policy",
        base::ToString(registration_criteria_->enabled_by_enterprise_policy)));
    props.push_back(mojom::BrokerPropertyInfo::New(
        "Enabled by user setting",
        base::ToString(registration_criteria_->enabled_by_user_setting)));
    props.push_back(mojom::BrokerPropertyInfo::New(
        "On external power",
        base::ToString(registration_criteria_->is_on_external_power)));
    props.push_back(mojom::BrokerPropertyInfo::New(
        "Disk space free",
        base::ToString(registration_criteria_->disk_space_free.InMiB()) +
            " MiB"));
  }
  return props;
}

std::vector<mojom::BrokerAssetInfoPtr>
OnDeviceModelComponentStateManager::GetBrokerAssets() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<mojom::BrokerAssetInfoPtr> assets;
  auto asset = mojom::BrokerAssetInfo::New();
  asset->name = "Base Model";
  if (state_) {
    asset->version = state_->GetComponentVersion().GetString();
  }
  switch (component_installer_state_) {
    case ComponentInstallerState::kNotRegistered:
      asset->state = mojom::BrokerAssetState::kNotInstalled;
      break;
    case ComponentInstallerState::kRegistering:
      asset->state = mojom::BrokerAssetState::kRegistering;
      break;
    case ComponentInstallerState::kRegistered:
      asset->state = mojom::BrokerAssetState::kBackgroundInstalling;
      break;
    case ComponentInstallerState::kOnDemandDownloading:
      asset->state = mojom::BrokerAssetState::kForegroundInstalling;
      break;
    case ComponentInstallerState::kInstalled:
      asset->state = mojom::BrokerAssetState::kReady;
      break;
    case ComponentInstallerState::kUninstalling:
      asset->state = mojom::BrokerAssetState::kUninstalling;
      break;
  }
  assets.push_back(std::move(asset));
  return assets;
}

void OnDeviceModelComponentStateManager::SetReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    const base::DictValue& manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_.reset();

  if (auto model_spec = GetOnDeviceBaseModelSpecFromManifest(
          manifest, performance_classifier_->GetPossibleHints())) {
    state_ = std::make_unique<OnDeviceModelComponentState>(install_dir, version,
                                                           *model_spec);
    bool is_new_installation =
        (component_installer_state_ == ComponentInstallerState::kRegistered ||
         component_installer_state_ ==
             ComponentInstallerState::kOnDemandDownloading);
    component_installer_state_ = ComponentInstallerState::kInstalled;
    base::UmaHistogramEnumeration(
        "OptimizationGuide.OnDeviceModel.InstalledModel",
        ConvertModelNameToEnum(model_spec->model_name));
    if (is_new_installation) {
      base::UmaHistogramEnumeration(
          "OptimizationGuide.OnDeviceModel.NewModelInstalled",
          ConvertModelNameToEnum(model_spec->model_name));
    }
  }

  NotifyStateChanged();
}

void OnDeviceModelComponentStateManager::InstallerRegistered(
    bool is_already_installed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_already_installed) {
    component_installer_state_ = ComponentInstallerState::kInstalled;
  } else {
    component_installer_state_ = ComponentInstallerState::kRegistered;
  }
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime." +
          GetUmaModelNameFromState(state_.get()),
      state_ != nullptr);
  UpdateRegistration();
}

void OnDeviceModelComponentStateManager::UninstallComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->ClearPref(model_execution::prefs::localstate::
                              kLastTimeEligibleForOnDeviceModelDownload);
  component_installer_state_ = ComponentInstallerState::kNotRegistered;
}

void OnDeviceModelComponentStateManager::OnPerformanceClassAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeBeginBackgroundModelDownload();
}

void OnDeviceModelComponentStateManager::
    OnGenAILocalFoundationalModelEnterprisePolicyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::
    OnGenAILocalFoundationalModelUserSettingChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name,
    bool is_first_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto feature = GetFeatureForUseCase(use_case_name);
  if (!feature) {
    return;
  }

  if (GetOnDeviceModelType(*feature) != model_type_) {
    return;
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelStatusAtUseTime",
      GetOnDeviceModelStatus());

  if (registration_criteria_) {
    LogInstallCriteria(*registration_criteria_, "AtAttemptedUse");
  }

  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::MaybeBeginBackgroundModelDownload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_download_requested_ = true;
  BeginUpdateRegistration();
}

void OnDeviceModelComponentStateManager::GetFreeDiskSpaceForLogging(
    base::OnceCallback<void(std::optional<base::ByteCount>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetFreeDiskSpace(delegate_->GetInstallDirectory(),
                              std::move(callback));
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
    registration_criteria_->enabled_by_user_setting = true;

    if (!state_) {
      SetReady(base::Version("override"), *model_path_override_switch,
               MakeOverrideManifest());
    }
    return;
  }
  delegate_->GetFreeDiskSpace(
      delegate_->GetInstallDirectory(),
      base::BindOnce(
          &OnDeviceModelComponentStateManager::UpdateRegistrationCriteria,
          GetWeakPtr()));
}

OnDeviceModelComponentStateManager::RegistrationCriteria
OnDeviceModelComponentStateManager::ComputeRegistrationCriteria(
    base::ByteCount disk_space_free_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegistrationCriteria result;
  result.background_download_requested = background_download_requested_;
  result.disk_space_free = disk_space_free_bytes;
  result.device_capable = performance_classifier_->IsDeviceCapable();
  result.on_device_feature_recently_used =
      WasOnDeviceModelRecentlyUsed(&usage_tracker_.get(), model_type_);
  result.enabled_by_feature = features::IsOnDeviceExecutionEnabled();
  result.enabled_by_enterprise_policy =
      GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state_) ==
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
  result.enabled_by_user_setting = local_state_->GetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled);

  // Treat a null PowerMonitor (for some tests) as being on battery power.
  result.is_on_external_power =
      base::PowerMonitor::GetInstance()->IsInitialized() &&
      !base::PowerMonitor::GetInstance()->IsOnBatteryPower();

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

void OnDeviceModelComponentStateManager::UpdateRegistrationCriteria(
    std::optional<base::ByteCount> disk_space_free) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/438265416): Handle failure to get free disk space.
  RegistrationCriteria criteria = ComputeRegistrationCriteria(
      disk_space_free.value_or(base::ByteCount(-1)));
  bool first_registration_attempt = !registration_criteria_;

  OnDeviceModelStatus status = GetOnDeviceModelStatus();
  registration_criteria_ = std::make_unique<RegistrationCriteria>(criteria);
  if (status != GetOnDeviceModelStatus()) {
    NotifyStateChanged();
  }

  if (criteria.get_install_mode().has_value()) {
    local_state_->SetTime(model_execution::prefs::localstate::
                              kLastTimeEligibleForOnDeviceModelDownload,
                          base::Time::Now());
  }

  // Log metrics only for first registration attempt.
  if (first_registration_attempt) {
    LogInstallCriteria(criteria, "AtRegistration",
                       disk_space_free.value_or(base::ByteCount(-1)).InGiB());
  }

  UpdateRegistration();
}

void OnDeviceModelComponentStateManager::UpdateRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(registration_criteria_);

  if (component_installer_state_ == ComponentInstallerState::kRegistering ||
      component_installer_state_ == ComponentInstallerState::kUninstalling) {
    // Can't do anything right now, wait for InstallerRegistered() /
    // UninstallComplete() for next action.
    return;
  }
  std::optional<RegistrationCriteria::UninstallReason> uninstall_reason =
      registration_criteria_->should_uninstall();
  if (uninstall_reason.has_value()) {
    // If `state_` is null, the uninstallation is happening before the model is
    // ready, so `Unknown` is logged.
    std::string uma_model_name = GetUmaModelNameFromState(state_.get());
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason." +
            uma_model_name,
        *uninstall_reason);

    component_installer_state_ = ComponentInstallerState::kUninstalling;
    // Uninstall the component which will delete the model files, after a
    // short delay to give time for the consumers to unload the model.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceModelComponentStateManager::UninstallComponent,
                       GetWeakPtr()),
        kUninstallDelay);
    return;
  }

  if (component_installer_state_ == ComponentInstallerState::kNotRegistered) {
    if (registration_criteria_->get_install_mode().has_value() ||
        registration_criteria_->is_already_installing) {
      component_installer_state_ = ComponentInstallerState::kRegistering;
      delegate_->RegisterInstaller(
          GetWeakPtr(), OnDeviceModelRegistrationAttributes(
                            base::flat_set<proto::OnDeviceModelPerformanceHint>(
                                performance_classifier_->GetPossibleHints())));
    }
    return;
  }

  if (component_installer_state_ == ComponentInstallerState::kRegistered) {
    if (registration_criteria_->get_install_mode() ==
        ModelInstallMode::kOnDemand) {
      component_installer_state_ =
          ComponentInstallerState::kOnDemandDownloading;
      delegate_->RequestUpdate(/*is_background=*/false);
    }
    return;
  }
  CHECK(component_installer_state_ ==
            ComponentInstallerState::kOnDemandDownloading ||
        component_installer_state_ == ComponentInstallerState::kInstalled);
}

void OnDeviceModelComponentStateManager::UninstallComponent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->Uninstall(GetWeakPtr());
}

void OnDeviceModelComponentStateManager::NotifyStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeOnDeviceModelComponentState state = GetOnDeviceModelState();
  for (auto& o : observers_) {
    o.StateChanged(state);
  }
}

void OnDeviceModelComponentStateManager::ForceUninstall() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/424764871): Likely will need to notify observers of the
  // state change.
  UninstallComponent();
}

MaybeOnDeviceModelComponentState
OnDeviceModelComponentStateManager::GetOnDeviceModelState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetState() != nullptr) {
    return std::cref(*GetState());
  }
  if (!registration_criteria_) {
    return base::unexpected(OnDeviceModelStatus::kNotReadyForUnknownReason);
  }
  if (!registration_criteria_->is_model_allowed()) {
    return base::unexpected(OnDeviceModelStatus::kNotEligible);
  }
  if (!registration_criteria_->is_disk_space_available()) {
    return base::unexpected(OnDeviceModelStatus::kInsufficientDiskSpace);
  }
  bool is_installing =
      component_installer_state_ != ComponentInstallerState::kNotRegistered &&
      component_installer_state_ != ComponentInstallerState::kUninstalling;
  if (is_installing) {
    return base::unexpected(OnDeviceModelStatus::kInstallNotComplete);
  }
  if (!registration_criteria_->on_device_feature_recently_used) {
    return base::unexpected(OnDeviceModelStatus::kNoOnDeviceFeatureUsed);
  }
  // This may happen before the first registration.
  return base::unexpected(
      OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason);
}

OnDeviceModelStatus
OnDeviceModelComponentStateManager::GetOnDeviceModelStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetOnDeviceModelState().error_or(OnDeviceModelStatus::kReady);
}

}  // namespace optimization_guide
