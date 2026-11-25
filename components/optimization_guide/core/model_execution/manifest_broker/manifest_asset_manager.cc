// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "components/optimization_guide/core/model_execution/component_download_observer.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/disk_space_status.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

BASE_FEATURE(kOnDeviceModelEviction, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kScanForOrphanedManifestAssets, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kOrphanedManifestAssetScanDelay,
                   &kScanForOrphanedManifestAssets,
                   "scan_delay",
                   base::Seconds(5));

namespace {
// Delay to give consumers time to unload the model before it's deleted.
constexpr base::TimeDelta kUninstallDelay = base::Seconds(1);

// Keys for the ledger dictionary.
constexpr char kAssetIdKey[] = "asset_id";
constexpr char kRequestedVersionKey[] = "requested_version";
constexpr char kInstallStateKey[] = "install_state";

// We use a dummy "requested version" for the ledger when we've started an
// uninstall of a version. This means it may still require cleanup, but we
// can't treat it as an installation of any real version.
constexpr char kUninstallingVersion[] = "uninstalling";

// Whether `component_update_service` is currently downloading or installing an
// update for `crx_id`. False if `component_update_service` is null or the
// component is unknown.
// Note: only the component updater's state is consulted, so a requested
// download is reported as `kDownloadable` (or `kReady` for an update) until
// the updater actually starts on it.
bool IsComponentUpdateInProgress(
    component_updater::ComponentUpdateService* component_update_service,
    const std::string& crx_id) {
  if (!component_update_service) {
    return false;
  }
  component_updater::CrxUpdateItem item;
  if (!component_update_service->GetComponentDetails(crx_id, &item)) {
    return false;
  }
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
      return true;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpToDate:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

bool IsBaseModelAsset(const Manifest& manifest, std::string_view asset_id) {
  return std::ranges::any_of(
      manifest.GetRecipes().base_models(), [&](const auto& entry) {
        return entry.second.weights_file().asset_id() == asset_id;
      });
}

// LINT.IfChange(OnDeviceModelUpdateErrorCategory)
std::string_view ErrorCategoryToHistogramVariant(
    update_client::ErrorCategory category) {
  switch (category) {
    case update_client::ErrorCategory::kNone:
      NOTREACHED();
    case update_client::ErrorCategory::kDownload:
      return "Download";
    case update_client::ErrorCategory::kUnpack:
      return "Unpack";
    case update_client::ErrorCategory::kInstall:
      return "Install";
    case update_client::ErrorCategory::kService:
      return "Service";
    case update_client::ErrorCategory::kUpdateCheck:
      return "UpdateCheck";
    case update_client::ErrorCategory::kInstaller:
      return "Installer";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/histograms.xml:OnDeviceModelUpdateErrorCategory)

std::optional<ManifestAssetManager::UpdateAttemptOutcome>
ClassifyUpdateAttemptOutcome(update_client::ComponentState state,
                             update_client::ErrorCategory error_category,
                             int error_code) {
  switch (state) {
    case update_client::ComponentState::kUpdated:
      return ManifestAssetManager::UpdateAttemptOutcome::kSucceeded;
    case update_client::ComponentState::kUpToDate:
      return ManifestAssetManager::UpdateAttemptOutcome::kUpToDate;
    case update_client::ComponentState::kUpdateError:
      break;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kRun:
      return std::nullopt;
  }

  if (error_category == update_client::ErrorCategory::kDownload &&
      error_code ==
          static_cast<int>(update_client::CrxDownloaderError::DISK_FULL)) {
    return ManifestAssetManager::UpdateAttemptOutcome::kDiskFull;
  }
  if ((error_category == update_client::ErrorCategory::kDownload &&
       error_code ==
           static_cast<int>(update_client::CrxDownloaderError::CANCELLED)) ||
      (error_category == update_client::ErrorCategory::kService &&
       error_code ==
           static_cast<int>(update_client::ServiceError::CANCELLED))) {
    return ManifestAssetManager::UpdateAttemptOutcome::kCancelled;
  }
  if (error_category == update_client::ErrorCategory::kService &&
      error_code ==
          static_cast<int>(update_client::ServiceError::UPDATE_DISABLED)) {
    return ManifestAssetManager::UpdateAttemptOutcome::kPolicyOrMeteredSkipped;
  }
  if (error_category == update_client::ErrorCategory::kInstall &&
      error_code ==
          static_cast<int>(
              update_client::InstallError::CLEAN_INSTALL_DIR_FAILED)) {
    return ManifestAssetManager::UpdateAttemptOutcome::kCleanInstallDirFailed;
  }

  switch (error_category) {
    case update_client::ErrorCategory::kDownload:
      return ManifestAssetManager::UpdateAttemptOutcome::kDownloadError;
    case update_client::ErrorCategory::kUnpack:
      return ManifestAssetManager::UpdateAttemptOutcome::kUnpackError;
    case update_client::ErrorCategory::kInstall:
    case update_client::ErrorCategory::kInstaller:
      return ManifestAssetManager::UpdateAttemptOutcome::kInstallError;
    case update_client::ErrorCategory::kUpdateCheck:
      return ManifestAssetManager::UpdateAttemptOutcome::kUpdateCheckError;
    case update_client::ErrorCategory::kService:
      return ManifestAssetManager::UpdateAttemptOutcome::kServiceError;
    case update_client::ErrorCategory::kNone:
      NOTREACHED();
  }
}

}  // namespace

ManifestAssetManager::ComponentContext::ComponentContext() = default;
ManifestAssetManager::ComponentContext::~ComponentContext() = default;
ManifestAssetManager::ComponentContext::ComponentContext(
    const ComponentContext&) = default;
ManifestAssetManager::ComponentContext&
ManifestAssetManager::ComponentContext::operator=(const ComponentContext&) =
    default;

bool ManifestAssetManager::ComponentContext::NeedsCleanup() const {
  return !requested_version_.empty();
}

ManifestSolutionFactory::AssetState
ManifestAssetManager::ComponentContext::AsAssetState(
    const std::string& version) const {
  if (state_ == ComponentState::kReady && install_dir_.has_value() &&
      version_.has_value() && version_->GetString() == version) {
    return ManifestSolutionFactory::AssetInfo{.path = *install_dir_};
  }
  return base::unexpected(
      ManifestSolutionFactory::AssetUnavailableReason::kNotDownloaded);
}

mojom::BrokerAssetState
ManifestAssetManager::ComponentContext::ToBrokerAssetState() const {
  switch (state_) {
    case ComponentState::kNotRegistered:
      return mojom::BrokerAssetState::kNotInstalled;
    case ComponentState::kRegistering:
      return mojom::BrokerAssetState::kRegistering;
    case ComponentState::kRegistered:
      return mojom::BrokerAssetState::kNotInstalled;
    case ComponentState::kOnDemandDownloading:
      return mojom::BrokerAssetState::kForegroundInstalling;
    case ComponentState::kReady:
      return mojom::BrokerAssetState::kReady;
    case ComponentState::kUninstalling:
      return mojom::BrokerAssetState::kUninstalling;
  }
}

mojom::BrokerAssetInfoPtr
ManifestAssetManager::ComponentContext::ToBrokerAssetInfo(
    const proto::OnDemandComponent* target) const {
  auto asset_info = mojom::BrokerAssetInfo::New();
  asset_info->name = asset_id_;
  if (target) {
    asset_info->version = target->target_version();
  } else {
    asset_info->version = requested_version_;
  }
  asset_info->state = ToBrokerAssetState();
  if (version_.has_value() && requested_version_ != version_->GetString()) {
    asset_info->error =
        base::StrCat({"Mismatched version: ", version_->GetString()});
  }
  return asset_info;
}

void ManifestAssetManager::ComponentContext::SetAssetId(
    const std::string& asset_id) {
  asset_id_ = asset_id;
}

bool ManifestAssetManager::ComponentContext::SetUninstallComplete() {
  // A registration could be mid-flight if this was uninstalled externally.
  // We still expect the new registration to finish in that case.
  if (state_ != ComponentState::kRegistering) {
    state_ = ComponentState::kNotRegistered;
    requested_version_ = "";
  }
  install_dir_ = std::nullopt;
  version_ = std::nullopt;
  const Manifest::UninstallReason reason =
      std::exchange(uninstall_reason_, Manifest::UninstallReason::kUnknown);
  // Keep `kNeverInstalled` when cleaning up incomplete registrations.
  if (install_state_ == OnDeviceModelControl::ModelInstallState::kInstalled) {
    install_state_ =
        reason == Manifest::UninstallReason::kObsolete
            ? OnDeviceModelControl::ModelInstallState::kUninstalledObsolete
            : OnDeviceModelControl::ModelInstallState::kUninstalled;
    return true;
  }
  return false;
}

void ManifestAssetManager::ComponentContext::SetRegistering(
    const std::string& target_version) {
  CHECK(state_ != ComponentState::kRegistering &&
        state_ != ComponentState::kUninstalling);
  state_ = ComponentState::kRegistering;
  requested_version_ = target_version;
  install_dir_ = std::nullopt;
  version_ = std::nullopt;
}

void ManifestAssetManager::ComponentContext::SetRegistered() {
  CHECK(state_ == ComponentState::kRegistering ||
        state_ == ComponentState::kReady);
  state_ = ComponentState::kRegistered;
  passive_registration_logged_ = false;
}

void ManifestAssetManager::ComponentContext::SetOnDemandDownloading() {
  CHECK_EQ(state_, ComponentState::kRegistered);
  state_ = ComponentState::kOnDemandDownloading;
}

void ManifestAssetManager::ComponentContext::SetReadySoon() {
  CHECK(state_ == ComponentState::kReady ||
        state_ == ComponentState::kRegistering);
  state_ = ComponentState::kReady;
}

OnDeviceModelControl::ModelInstallState
ManifestAssetManager::ComponentContext::SetReady(
    const base::FilePath& install_dir,
    const base::Version& version) {
  CHECK(state_ == ComponentState::kReady ||
        state_ == ComponentState::kRegistering ||
        state_ == ComponentState::kRegistered ||
        state_ == ComponentState::kOnDemandDownloading);
  state_ = ComponentState::kReady;
  install_dir_ = install_dir;
  version_ = version;
  return std::exchange(install_state_,
                       OnDeviceModelControl::ModelInstallState::kInstalled);
}

void ManifestAssetManager::ComponentContext::SetUninstalling(
    Manifest::UninstallReason reason) {
  CHECK(state_ != ComponentState::kRegistering &&
        state_ != ComponentState::kUninstalling);
  state_ = ComponentState::kUninstalling;
  requested_version_ = kUninstallingVersion;
  install_dir_ = std::nullopt;
  version_ = std::nullopt;
  uninstall_reason_ = reason;
}

void ManifestAssetManager::ComponentContext::SetOrphaned(
    const std::optional<base::Version>& version) {
  CHECK_EQ(state_, ComponentState::kNotRegistered);
  CHECK(requested_version_.empty());
  CHECK(!version || version->IsValid());
  requested_version_ =
      version ? version->GetString() : std::string(kUninstallingVersion);
}

ManifestAssetManager::ComponentContext
ManifestAssetManager::ComponentContext::FromValue(
    const base::DictValue& value) {
  ComponentContext context;
  if (const std::string* id = value.FindString(kAssetIdKey)) {
    context.asset_id_ = *id;
  }
  if (const std::string* v = value.FindString(kRequestedVersionKey)) {
    context.requested_version_ = *v;
  }
  if (std::optional<int> state = value.FindInt(kInstallStateKey)) {
    // Already migrated; read the persisted `ModelInstallState`.
    if (*state >= 0 &&
        *state <= static_cast<int>(
                      OnDeviceModelControl::ModelInstallState::kMaxValue)) {
      context.install_state_ =
          static_cast<OnDeviceModelControl::ModelInstallState>(*state);
    }
    return context;
  }
  // Populate missing `install_state` as `kInstalled` for active entries.
  if (!context.requested_version_.empty() &&
      context.requested_version_ != kUninstallingVersion) {
    context.install_state_ =
        OnDeviceModelControl::ModelInstallState::kInstalled;
  }
  return context;
}

base::DictValue ManifestAssetManager::ComponentContext::ToValue() const {
  base::DictValue dict;
  dict.Set(kAssetIdKey, asset_id_);
  dict.Set(kRequestedVersionKey, requested_version_);
  dict.Set(kInstallStateKey, static_cast<int>(install_state_));
  return dict;
}

ManifestAssetManager::AssetLedger::AssetLedger(PrefService& local_state)
    : local_state_(local_state) {}

ManifestAssetManager::AssetLedger::~AssetLedger() = default;

void ManifestAssetManager::AssetLedger::Load() {
  component_contexts_.clear();
  const base::DictValue& ledger_dict = local_state_->GetDict(
      model_execution::prefs::localstate::kManifestAssetLedger);
  for (auto it : ledger_dict) {
    const std::string& public_key = it.first;
    const base::DictValue* entry_dict = it.second.GetIfDict();
    if (!entry_dict) {
      continue;
    }
    component_contexts_.emplace(public_key,
                                ComponentContext::FromValue(*entry_dict));
  }
}

const ManifestAssetManager::ComponentContext*
ManifestAssetManager::AssetLedger::GetContext(
    const std::string& public_key) const {
  auto it = contexts().find(public_key);
  if (it == component_contexts_.end()) {
    return nullptr;
  }
  return &it->second;
}

ManifestAssetManager::ComponentContext*
ManifestAssetManager::AssetLedger::GetContext(const std::string& public_key) {
  return GetContextImpl(public_key, /*create_if_missing=*/false);
}

ManifestAssetManager::ComponentContext*
ManifestAssetManager::AssetLedger::GetOrCreateContext(
    const std::string& public_key) {
  return GetContextImpl(public_key, /*create_if_missing=*/true);
}

ManifestAssetManager::ComponentContext*
ManifestAssetManager::AssetLedger::GetContextImpl(const std::string& public_key,
                                                  bool create_if_missing) {
  auto it = component_contexts_.find(public_key);
  if (it == component_contexts_.end()) {
    if (create_if_missing) {
      return &component_contexts_[public_key];
    }
    return nullptr;
  }
  return &it->second;
}

void ManifestAssetManager::AssetLedger::SaveContexts(
    const std::vector<std::string>& public_keys) {
  ScopedDictPrefUpdate update(
      local_state_.get(),
      model_execution::prefs::localstate::kManifestAssetLedger);
  for (const std::string& public_key : public_keys) {
    const ComponentContext* context = GetContext(public_key);
    if (!context) {
      continue;
    }
    update->Set(public_key, context->ToValue());
  }
}

OnDeviceModelControl::ModelInstallState
ManifestAssetManager::AssetLedger::GetModelInstallState(
    const PrefService& local_state,
    OnDeviceBaseModel model) {
  if (model == OnDeviceBaseModel::kUnknown) {
    return ModelInstallState::kNeverInstalled;
  }
  ModelInstallState result = ModelInstallState::kNeverInstalled;
  for (const auto [_, value] : local_state.GetDict(
           model_execution::prefs::localstate::kManifestAssetLedger)) {
    const base::DictValue* dict = value.GetIfDict();
    if (!dict) {
      continue;
    }
    ComponentContext context = ComponentContext::FromValue(*dict);
    if (ConvertComponentKeyToEnum(context.asset_id()) != model) {
      continue;
    }
    if (context.install_state() == ModelInstallState::kInstalled) {
      return ModelInstallState::kInstalled;
    }
    if (context.install_state() == ModelInstallState::kUninstalledObsolete) {
      result = ModelInstallState::kUninstalledObsolete;
    } else if (context.install_state() == ModelInstallState::kUninstalled &&
               result == ModelInstallState::kNeverInstalled) {
      result = ModelInstallState::kUninstalled;
    }
  }
  return result;
}

ManifestAssetManager::ManifestAssetManager(
    PrefService& local_state,
    UsageTracker& usage_tracker,
    ManifestAssetManagerDelegate& delegate,
    component_updater::ComponentUpdateService* component_update_service,
    base::ObserverList<OnDeviceModelControl::Observer>& model_observers,
    std::unique_ptr<ManifestSolutionFactory> factory)
    : local_state_(local_state),
      usage_tracker_(usage_tracker),
      delegate_(delegate),
      component_update_service_(component_update_service),
      model_observers_(model_observers),
      ledger_(local_state),
      disk_space_status_(delegate) {
  delegate_->SetAssetManager(weak_ptr_factory_.GetWeakPtr());

  // Load persistent state from the ledger immediately on startup.
  ledger_.Load();

  // Update with the initial factory. This will call UpdateRegistration.
  UpdateSolutionFactory(std::move(factory));

  // Register observers for when we might need to update the registrations.
  usage_tracker_observation_.Observe(&usage_tracker);
  pref_change_registrar_.Init(&local_state_.get());
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::
          kOnDeviceAiAutoRemoveUnusedModelsEnabled,
      base::BindRepeating(&ManifestAssetManager::UpdateRegistrations,
                          weak_ptr_factory_.GetWeakPtr()));

  // Schedule a one-time scan for orphaned assets on disk after initialization.
  if (base::FeatureList::IsEnabled(kScanForOrphanedManifestAssets)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ManifestAssetManager::ScanForOrphanedAssets,
                       weak_ptr_factory_.GetWeakPtr()),
        kOrphanedManifestAssetScanDelay.Get());
  }
}

ManifestAssetManager::~ManifestAssetManager() {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::~ManifestAssetManager",
              perfetto::TerminatingFlow::FromPointer(this));
  delegate_->SetDiskSpaceCheckPriority(
      ManifestMonitorDelegate::DiskSpaceCheckReason::kAssetRegistration,
      /*user_visible=*/false);
}

void ManifestAssetManager::RecordRegistrationMode(const std::string& asset_id,
                                                  RegistrationMode mode) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsBaseModelAsset(factory_->manifest(), asset_id)) {
    return;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceModelRegistrationMode.",
                    ConvertComponentKeyToUmaModelName(asset_id)}),
      mode);
}

void ManifestAssetManager::OnAssetUpdateEvent(
    const std::string& public_key,
    update_client::ComponentState state,
    update_client::ErrorCategory error_category,
    int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<UpdateAttemptOutcome> outcome =
      ClassifyUpdateAttemptOutcome(state, error_category, error_code);
  if (!outcome) {
    return;
  }
  const ComponentContext* context = ledger_.GetContext(public_key);
  if (!context || context->state() == ComponentState::kNotRegistered ||
      context->state() == ComponentState::kUninstalling ||
      !IsBaseModelAsset(factory_->manifest(), context->asset_id())) {
    return;
  }
  std::string model_name =
      ConvertComponentKeyToUmaModelName(context->asset_id());
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceModelUpdateAttemptOutcome.",
                    model_name}),
      *outcome);
  if (state == update_client::ComponentState::kUpdateError) {
    base::UmaHistogramSparse(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceModelUpdateErrorCode.",
                      ErrorCategoryToHistogramVariant(error_category), ".",
                      model_name}),
        error_code);
  }
}

std::optional<std::string> ManifestAssetManager::GetCrxIdForAsset(
    const std::string& asset_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!factory_) {
    return std::nullopt;
  }

  const auto& on_demand_components =
      factory_->manifest().GetAssets().on_demand_components();
  auto it = on_demand_components.find(asset_name);
  if (it == on_demand_components.end()) {
    return std::nullopt;
  }

  return GetCrxIdFromPublicKeyHex(it->second.public_key());
}

std::optional<base::flat_set<std::string>>
ManifestAssetManager::GetRequiredComponentIdsForUseCase(
    const std::string& use_case) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!factory_) {
    return std::nullopt;
  }

  base::optional_ref<const absl::flat_hash_set<Manifest::AssetId>>
      required_assets = factory_->manifest().GetRequiredAssets(use_case);
  if (!required_assets) {
    return std::nullopt;
  }

  base::flat_set<std::string> component_ids;
  for (const auto& asset_id : *required_assets) {
    if (std::optional<std::string> crx_id = GetCrxIdForAsset(asset_id)) {
      component_ids.insert(*crx_id);
    }
  }
  return component_ids;
}

void ManifestAssetManager::AddDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::flat_set<std::string>> component_ids =
      GetRequiredComponentIdsForUseCase(use_case);
  if (!component_ids) {
    return;
  }

  auto& entry = progress_managers_[use_case];
  if (!entry.manager) {
    entry.component_ids = *component_ids;
    entry.manager = std::make_unique<OnDeviceModelDownloadProgressManager>(
        component_update_service_, std::move(*component_ids));
  }
  entry.manager->AddObserver(std::move(observer));
}

void ManifestAssetManager::AddAssetDownloadObserver(
    const std::string& asset_name,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!component_update_service_) {
    return;
  }

  auto crx_id = GetCrxIdForAsset(asset_name);
  if (!crx_id) {
    return;
  }

  auto& tracker = asset_download_observers_[*crx_id];
  if (!tracker) {
    tracker = std::make_unique<ComponentDownloadObserver>(
        component_update_service_, *crx_id);
  }
  tracker->AddObserver(std::move(observer));
}

void ManifestAssetManager::UpdateSolutionFactory(
    std::unique_ptr<ManifestSolutionFactory> factory) {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::UpdateSolutionFactory",
              perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(holte): Potentially defer stopping the old factory from providing new
  // solutions until we actually download assets for the new factory.
  factory_ = std::move(factory);
  std::erase_if(progress_managers_, [&](const auto& item) {
    const auto& [use_case, entry] = item;
    return GetRequiredComponentIdsForUseCase(use_case) != entry.component_ids;
  });
  if (std::optional<base::ByteSize> free_space =
          disk_space_status_.free_space()) {
    factory_->UpdateFreeDiskSpace(free_space);
  }

  // Mark the manifest asset as ready. This is deferred until now let the
  // AssetManager decide when the factory can start providing solutions.
  factory_->UpdateAssetState(kManifestAssetName,
                             ManifestSolutionFactory::AssetInfo{
                                 .path = factory_->manifest().GetDirectory()});

  // Make sure the ledger tracks all assets in the new manifest.
  std::vector<std::string> keys_to_save;
  for (const auto& [asset_id, component] :
       factory_->manifest().GetAssets().on_demand_components()) {
    ComponentContext& context =
        *ledger_.GetOrCreateContext(component.public_key());
    if (context.asset_id() != asset_id &&
        (context.NeedsCleanup() ||
         context.install_state() !=
             OnDeviceModelControl::ModelInstallState::kNeverInstalled)) {
      keys_to_save.push_back(component.public_key());
    }
    context.SetAssetId(asset_id);
  }
  if (!keys_to_save.empty()) {
    ledger_.SaveContexts(keys_to_save);
  }

  RecomputeAssetPriorities();
}

void ManifestAssetManager::RefreshSolutions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (factory_) {
    factory_->UpdateSolutions();
  }
}

void ManifestAssetManager::OnPriorityIncrease(
    const std::string& use_case_name,
    UsageTracker::Priority previous_priority) {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::OnPriorityIncrease",
              perfetto::Flow::FromPointer(this), "use_case_name", use_case_name);
  RecomputeAssetPriorities();
}

void ManifestAssetManager::RecomputeAssetPriorities() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  asset_priorities_ = AssetPriorities(factory_->manifest(), *usage_tracker_);
  UpdateRegistrations();
}

void ManifestAssetManager::UninstallAllAssets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [use_case_name, _] :
       factory_->manifest().GetDeviceCategoryConfig().use_cases()) {
    usage_tracker_->RequestUninstall(use_case_name);
  }
  RecomputeAssetPriorities();
}

void ManifestAssetManager::InstallCapability(OnDeviceBaseModel base_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base_model == OnDeviceBaseModel::kUnknown) {
    return;
  }
  for (const auto& [asset_id, component] :
       factory_->manifest().GetAssets().on_demand_components()) {
    if (ConvertComponentKeyToEnum(asset_id) != base_model ||
        ComputeModelStatus(ledger_.GetContext(component.public_key()),
                           component) !=
            OnDeviceModelControl::ModelStatus::kDownloadable) {
      continue;
    }
    for (const auto& use_case_name :
         factory_->manifest().GetUseCasesRequiringAsset(asset_id)) {
      usage_tracker_->RaisePriority(use_case_name,
                                    UsageTracker::Priority::kUserBlocking);
    }
  }
}

void ManifestAssetManager::UninstallCapability(OnDeviceBaseModel base_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base_model == OnDeviceBaseModel::kUnknown) {
    return;
  }
  bool any_uninstalled = false;
  for (const auto& [asset_id, component] :
       factory_->manifest().GetAssets().on_demand_components()) {
    if (ConvertComponentKeyToEnum(asset_id) != base_model ||
        ComputeModelStatus(ledger_.GetContext(component.public_key()),
                           component) !=
            OnDeviceModelControl::ModelStatus::kReady) {
      continue;
    }
    for (const auto& use_case_name :
         factory_->manifest().GetUseCasesRequiringAsset(asset_id)) {
      usage_tracker_->RequestUninstall(use_case_name);
      any_uninstalled = true;
    }
  }
  if (any_uninstalled) {
    RecomputeAssetPriorities();
  }
}

bool ManifestAssetManager::IsEvictionEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FeatureList::IsEnabled(kOnDeviceModelEviction) &&
         local_state_->GetBoolean(model_execution::prefs::localstate::
                                      kOnDeviceAiAutoRemoveUnusedModelsEnabled);
}

void ManifestAssetManager::ScanForOrphanedAssets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetInstalledAssets(
      base::BindOnce(&ManifestAssetManager::OnOrphanedAssetsFound,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ManifestAssetManager::OnOrphanedAssetsFound(
    std::vector<ManifestAssetManagerDelegate::InstalledAsset>
        installed_assets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> keys_to_save;
  for (const auto& asset : installed_assets) {
    ComponentContext& context =
        *ledger_.GetOrCreateContext(asset.public_key_hex);
    if (!context.NeedsCleanup()) {
      context.SetOrphaned(asset.version);
      keys_to_save.push_back(asset.public_key_hex);
    }
  }
  if (!keys_to_save.empty()) {
    ledger_.SaveContexts(keys_to_save);
    UpdateRegistrations();
  }
}

void ManifestAssetManager::OnDiskSpaceEvaluated() {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::OnDiskSpaceEvaluated",
              perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  factory_->UpdateFreeDiskSpace(disk_space_status_.free_space());
  UpdateRegistrations();
}

bool ManifestAssetManager::CanSupportProactiveDownload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kOnDeviceModelBackgroundDownload)) {
    return false;
  }
  if (!base::PowerMonitor::GetInstance()->IsInitialized() ||
      base::PowerMonitor::GetInstance()->IsOnBatteryPower()) {
    return false;
  }
  return disk_space_status_.IsSufficientForBackgroundInstall();
}

bool ManifestAssetManager::ShouldRetain(const ComponentContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!asset_priorities_.IsAtLeast(AssetPriority::kEvictable,
                                   context.asset_id())) {
    return false;
  }
  // If the component already has the target version requested (downloading or
  // installed), retain it unless eviction is enabled and its priority has
  // dropped below kRetain.
  return !IsEvictionEnabled() ||
         asset_priorities_.IsAtLeast(AssetPriority::kRetain,
                                     context.asset_id());
}

bool ManifestAssetManager::ShouldRegisterNewVersion(
    const ComponentContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(disk_space_status_.IsFresh());
  if (!disk_space_status_.IsSufficientForOnDemandInstall()) {
    if (std::optional<base::ByteSize> free_space =
            disk_space_status_.free_space()) {
      base::UmaHistogramCounts100(
          "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
          "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
          free_space->InGiB());
    }
    return false;
  }
  return asset_priorities_.IsAtLeast(AssetPriority::kBestEffort,
                                     context.asset_id()) ||
         (CanSupportProactiveDownload() &&
          asset_priorities_.IsAtLeast(AssetPriority::kSpeculative,
                                      context.asset_id()));
}

bool ManifestAssetManager::ShouldInstallOrRetain(
    const ComponentContext& context,
    const proto::OnDemandComponent* component) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(disk_space_status_.IsFresh());
  if (!component) {
    return false;
  }
#if BUILDFLAG(CHROME_FOR_TESTING)
  // In Chrome for Testing the normal component update mechanism is disabled, so
  // the only components that are installed are the ones listed in the Chrome
  // for Testing configuration file as Required Components, see
  // 'docs/chrome_for_testing/chrome_for_testing_configuration.md'.
  //
  // The required components are installed before the browser starts, so here we
  // unconditionally allow installation of all on-demand components present in
  // the manifest and the actual filtering is done in
  // RequiredComponentsController, see
  // components/component_updater/required_components_controller.h.
  return true;
#else
  return context.requested_version() == component->target_version()
             ? ShouldRetain(context)
             : ShouldRegisterNewVersion(context);
#endif
}

bool ManifestAssetManager::ShouldRequestForegroundDownload(
    const ComponentContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return asset_priorities_.IsAtLeast(AssetPriority::kUserBlocking,
                                     context.asset_id());
}

void ManifestAssetManager::RecordEvictableAssetsCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int evictable_assets_count = 0;
  for (const auto& [public_key, context] : ledger_.contexts()) {
    const proto::OnDemandComponent* component =
        factory_->manifest().GetAssetByPublicKey(public_key);
    if (component &&
        context.requested_version() == component->target_version() &&
        // Exclude assets explicitly requested to be uninstalled.
        asset_priorities_.IsAtLeast(AssetPriority::kEvictable,
                                    context.asset_id()) &&
        !asset_priorities_.IsAtLeast(AssetPriority::kRetain,
                                     context.asset_id())) {
      evictable_assets_count++;
    }
  }
  base::UmaHistogramCounts100(
      "OptimizationGuide.ModelExecution.OnDeviceModelEvictableAssetsCount",
      evictable_assets_count);
}

Manifest::UninstallReason ManifestAssetManager::RecordUninstallReason(
    const ComponentContext& context,
    const proto::OnDemandComponent* component) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Manifest::UninstallReason log_reason =
      factory_->manifest().uninstall_reason();
  if (component && context.requested_version() == component->target_version()) {
    log_reason = !asset_priorities_.IsAtLeast(AssetPriority::kEvictable,
                                              context.asset_id())
                     ? Manifest::UninstallReason::kUserRequested
                     : Manifest::UninstallReason::kEvicted;
  }
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason." +
          ConvertComponentKeyToUmaModelName(context.asset_id()),
      log_reason);
  return log_reason;
}

bool ManifestAssetManager::AnyAssetIsUserBlockingAndNotReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [asset_id, component] :
       factory_->manifest().GetAssets().on_demand_components()) {
    if (!asset_priorities_.IsAtLeast(AssetPriority::kUserBlocking, asset_id)) {
      continue;
    }
    const auto* context = ledger_.GetContext(component.public_key());
    CHECK(context);
    if (!context->IsReadyFor(component.target_version())) {
      return true;
    }
  }
  return false;
}

void ManifestAssetManager::UpdateRegistrations() {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::UpdateRegistrations",
              perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool needs_disk_evaluation = !disk_space_status_.IsFresh();
  const bool needs_foreground_evaluation =
      needs_disk_evaluation && AnyAssetIsUserBlockingAndNotReady();
  delegate_->SetDiskSpaceCheckPriority(
      ManifestMonitorDelegate::DiskSpaceCheckReason::kAssetRegistration,
      needs_foreground_evaluation);
  if (needs_disk_evaluation) {
    // Don't do anything yet, just refresh the disk space status.
    if (!disk_space_status_.IsEvaluationPending()) {
      disk_space_status_.EnsureEvaluated(
          base::BindOnce(&ManifestAssetManager::OnDiskSpaceEvaluated,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  RecordEvictableAssetsCount();

  std::vector<std::string> keys_to_save;
  for (auto& [public_key, context] : ledger_.GetMutableContexts()) {
    if (context.state() == ComponentState::kRegistering ||
        context.state() == ComponentState::kUninstalling) {
      // Can't do anything right now during
      // registering/uninstalling, wait for callbacks.
      continue;
    }
    const proto::OnDemandComponent* component =
        factory_->manifest().GetAssetByPublicKey(public_key);
    if (!ShouldInstallOrRetain(context, component)) {
      if (context.NeedsCleanup()) {
        context.SetUninstalling(RecordUninstallReason(context, component));

        keys_to_save.push_back(public_key);
        // Uninstall the component which will delete the model files, after a
        // short delay to give time for the consumers to unload the model.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ManifestAssetManager::UninstallComponent,
                           weak_ptr_factory_.GetWeakPtr(), public_key),
            kUninstallDelay);
      }
      if (component) {
        NotifyFactory(public_key, context);
      }
      continue;
    }

    if (context.state() == ComponentState::kNotRegistered ||
        context.requested_version() != component->target_version()) {
      context.SetRegistering(component->target_version());
      keys_to_save.push_back(public_key);
      delegate_->RegisterOnDemandComponent(
          public_key, component->target_version(), context.asset_id());
      // Defer calling NotifyFactory until registration completes (via
      // InstallerRegistered or OnAssetReady), because the asset may already be
      // present on disk.
      continue;
    }

    if (context.state() == ComponentState::kReady &&
        !context.install_dir().has_value()) {
      // Registration completed recently and we've observed that we have the
      // right version, but we are waiting for the path from the updater.
      continue;
    }

    // Registration is complete, so we know the state of the asset and can
    // notify the factory.
    NotifyFactory(public_key, context);

    if (context.state() == ComponentState::kRegistered) {
      if (ShouldRequestForegroundDownload(context)) {
        context.SetOnDemandDownloading();
        RecordRegistrationMode(context.asset_id(),
                               RegistrationMode::kForegroundOnDemand);
        // This doesn't change a persistent state, so it's okay to not save.
        delegate_->RequestUpdate(public_key,
                                 /*is_background=*/false);
      } else if (!context.passive_registration_logged()) {
        context.set_passive_registration_logged();
        RecordRegistrationMode(context.asset_id(),
                               RegistrationMode::kPassiveBackground);
      }
      continue;
    }
    CHECK(context.state() == ComponentState::kOnDemandDownloading ||
          context.state() == ComponentState::kReady);
  }
  if (!keys_to_save.empty()) {
    ledger_.SaveContexts(keys_to_save);
  }
}

void ManifestAssetManager::UninstallComponent(const std::string& public_key) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::UninstallComponent",
              perfetto::Flow::FromPointer(this), "public_key", public_key);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->Uninstall(public_key);
}

void ManifestAssetManager::OnAssetUninstalled(const std::string& public_key) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::OnAssetUninstalled",
              perfetto::Flow::FromPointer(this), "public_key", public_key);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentContext* context = ledger_.GetContext(public_key);
  if (!context) {
    // The component is not tracked by this manager, so there is no state to
    // update.
    return;
  }
  // The uninstall may have been requested by this manager or by something
  // else (e.g. the component updater unregistering the component).
  const bool was_installed = context->SetUninstallComplete();
  ledger_.SaveContexts({public_key});
  const proto::OnDemandComponent* component =
      factory_->manifest().GetAssetByPublicKey(public_key);
  if (component) {
    UpdateRegistrations();
  }
  const OnDeviceBaseModel model_enum =
      ConvertComponentKeyToEnum(context->asset_id());
  if (was_installed && model_enum != OnDeviceBaseModel::kUnknown) {
    model_observers_->Notify(
        &OnDeviceModelControl::Observer::OnModelUninstalled, model_enum);
  }
}

void ManifestAssetManager::InstallerRegistered(const std::string& public_key,
                                               const std::string& version,
                                               bool is_already_installed) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::InstallerRegistered",
              perfetto::Flow::FromPointer(this), "public_key", public_key,
              "version", version, "is_already_installed", is_already_installed);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentContext* context = ledger_.GetContext(public_key);
  CHECK(context);  // Any asset that is registered should be in the ledger.
  if (context->state() == ComponentState::kUninstalling) {
    return;
  }
  if (is_already_installed) {
    context->SetReadySoon();
  } else {
    context->SetRegistered();
  }
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime." +
          ConvertComponentKeyToUmaModelName(context->asset_id()),
      is_already_installed);

  const proto::OnDemandComponent* component =
      factory_->manifest().GetAssetByPublicKey(public_key);
  if (component) {
    if (component->target_version() != version) {
      // We know we don't have the right version.
      NotifyFactory(public_key, *context);
    } else if (!is_already_installed) {
      // We know we don't have it installed.
      NotifyFactory(public_key, *context);
    }
    // Otherwise, we already have the right version installed, so we can wait
    // for OnAssetReady.
  }
  UpdateRegistrations();
}

void ManifestAssetManager::OnAssetReady(const std::string& public_key,
                                        const base::Version& version,
                                        const base::FilePath& install_dir) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::OnAssetReady",
              perfetto::Flow::FromPointer(this), "public_key", public_key);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentContext* context = ledger_.GetContext(public_key);
  CHECK(context);  // Any asset that is ready should be in the ledger.
  if (context->state() == ComponentState::kUninstalling) {
    return;
  }
  bool is_new_installation =
      (context->state() == ComponentState::kRegistered ||
       context->state() == ComponentState::kOnDemandDownloading);
  const OnDeviceModelControl::ModelInstallState previous_install_state =
      context->SetReady(install_dir, version);
  if (previous_install_state !=
      OnDeviceModelControl::ModelInstallState::kInstalled) {
    ledger_.SaveContexts({public_key});
  }

  OnDeviceBaseModel model_enum = ConvertComponentKeyToEnum(context->asset_id());
  base::UmaHistogramEnumeration(
      "OptimizationGuide.OnDeviceModel.InstalledModel", model_enum);
  if (is_new_installation) {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.OnDeviceModel.NewModelInstalled", model_enum);
  }

  NotifyFactory(public_key, *context);
  if (is_new_installation && model_enum != OnDeviceBaseModel::kUnknown &&
      previous_install_state !=
          OnDeviceModelControl::ModelInstallState::kInstalled) {
    model_observers_->Notify(&OnDeviceModelControl::Observer::OnModelInstalled,
                             model_enum);
  }
}

void ManifestAssetManager::OnUpdateRequestComplete(
    const std::string& public_key,
    update_client::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ComponentContext* context = ledger_.GetContext(public_key);
  if (!context || context->state() == ComponentState::kNotRegistered ||
      context->state() == ComponentState::kUninstalling ||
      !IsBaseModelAsset(factory_->manifest(), context->asset_id())) {
    return;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceModelOnDemandUpdateResult.",
                    ConvertComponentKeyToUmaModelName(context->asset_id())}),
      error);
}

void ManifestAssetManager::OnMeteredConnectionChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ManifestAssetManager::NotifyFactory(const std::string& public_key,
                                         const ComponentContext& context) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::NotifyFactory",
              perfetto::Flow::FromPointer(this), "public_key", public_key);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const proto::OnDemandComponent* component =
      factory_->manifest().GetAssetByPublicKey(public_key);
  if (!component) {
    // The factory doesn't care about this component.
    return;
  }
  factory_->UpdateAssetState(context.asset_id(),
                             context.AsAssetState(component->target_version()));
}

OnDeviceModelControl::ModelStatus ManifestAssetManager::ComputeModelStatus(
    const ComponentContext* context,
    const proto::OnDemandComponent& component) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context ||
      !asset_priorities_.IsAtLeast(AssetPriority::kEvictable,
                                   context->asset_id()) ||
      context->state() == ComponentState::kNotRegistered ||
      context->state() == ComponentState::kUninstalling) {
    return OnDeviceModelControl::ModelStatus::kDownloadable;
  }
  if (context->IsReadyFor(component.target_version())) {
    return OnDeviceModelControl::ModelStatus::kReady;
  }
  const std::optional<std::string> crx_id =
      GetCrxIdFromPublicKeyHex(component.public_key());
  const bool in_progress =
      crx_id && IsComponentUpdateInProgress(component_update_service_, *crx_id);
  if (context->has_installed_version()) {
    return in_progress ? OnDeviceModelControl::ModelStatus::kUpdating
                       : OnDeviceModelControl::ModelStatus::kReady;
  }
  return in_progress ? OnDeviceModelControl::ModelStatus::kDownloading
                     : OnDeviceModelControl::ModelStatus::kDownloadable;
}

std::vector<mojom::BrokerAssetInfoPtr> ManifestAssetManager::GetBrokerAssets()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<mojom::BrokerAssetInfoPtr> assets;
  for (const auto& [public_key, context] : ledger_.contexts()) {
    const proto::OnDemandComponent* component =
        factory_->manifest().GetAssetByPublicKey(public_key);
    if (!component && !context.NeedsCleanup()) {
      continue;
    }
    auto asset_info = context.ToBrokerAssetInfo(component);

    // Fetch initial download progress if available.
    if (component_update_service_) {
      if (std::optional<std::string> crx_id =
              GetCrxIdFromPublicKeyHex(public_key)) {
        if (auto progress =
                GetDownloadProgress(component_update_service_, *crx_id)) {
          asset_info->bytes_downloaded = progress->first;
          asset_info->bytes_total = progress->second;
        }
      }
    }

    assets.push_back(std::move(asset_info));
  }
  return assets;
}

std::vector<std::pair<mojom::BrokerModelInfoPtr, base::FilePath>>
ManifestAssetManager::GetBrokerModels() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return factory_->GetBrokerModels();
}

std::vector<std::pair<OnDeviceModelControl::ModelInfo, base::FilePath>>
ManifestAssetManager::GetBaseModels() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!factory_) {
    return {};
  }

  const Manifest& manifest = factory_->manifest();
  base::flat_map<OnDeviceBaseModel,
                 std::pair<OnDeviceModelControl::ModelInfo, base::FilePath>>
      models_by_base_model;

  for (const auto& [_, recipe] : manifest.GetRecipes().base_models()) {
    const Manifest::AssetId& asset_id = recipe.weights_file().asset_id();
    OnDeviceBaseModel base_model = ConvertComponentKeyToEnum(asset_id);
    if (base_model == OnDeviceBaseModel::kUnknown ||
        models_by_base_model.contains(base_model)) {
      continue;
    }
    const proto::OnDemandComponent* component = manifest.GetAssetById(asset_id);
    if (!component) {
      continue;
    }
    const ComponentContext* context =
        ledger_.GetContext(component->public_key());
    OnDeviceModelControl::ModelStatus status =
        ComputeModelStatus(context, *component);
    base::FilePath install_dir =
        status == OnDeviceModelControl::ModelStatus::kReady
            ? *context->install_dir()
            : base::FilePath();
    models_by_base_model.emplace(
        base_model, std::make_pair(
                        OnDeviceModelControl::ModelInfo{
                            .base_model = base_model,
                            .status = status,
                        },
                        std::move(install_dir)));
  }

  std::vector<std::pair<OnDeviceModelControl::ModelInfo, base::FilePath>>
      result;
  result.reserve(models_by_base_model.size());
  for (auto& [_, entry] : models_by_base_model) {
    result.push_back(std::move(entry));
  }
  return result;
}

OnDeviceModelControl::ModelInstallState
ManifestAssetManager::GetModelInstallState(const PrefService& local_state,
                                           OnDeviceBaseModel base_model) {
  return AssetLedger::GetModelInstallState(local_state, base_model);
}

}  // namespace optimization_guide
