// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

namespace {
// TTL for disk space evaluation result.
constexpr base::TimeDelta kDiskSpaceFreshnessThreshold = base::Seconds(10);

// Delay to give consumers time to unload the model before it's deleted.
constexpr base::TimeDelta kUninstallDelay = base::Seconds(1);

// Keys for the ledger dictionary.
constexpr char kAssetIdKey[] = "asset_id";
constexpr char kRequestedVersionKey[] = "requested_version";

// We use a dummy "requested version" for the ledger when we've started an
// uninstall of a version. This means it may still require cleanup, but we
// can't treat it as an installation of any real version.
constexpr char kUninstallingVersion[] = "uninstalling";
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

void ManifestAssetManager::ComponentContext::SetUninstallComplete() {
  CHECK_EQ(state_, ComponentState::kUninstalling);
  state_ = ComponentState::kNotRegistered;
  requested_version_ = "";
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

void ManifestAssetManager::ComponentContext::SetReady(
    const base::FilePath& install_dir,
    const base::Version& version) {
  CHECK(state_ == ComponentState::kReady ||
        state_ == ComponentState::kRegistering ||
        state_ == ComponentState::kRegistered ||
        state_ == ComponentState::kOnDemandDownloading);
  state_ = ComponentState::kReady;
  install_dir_ = install_dir;
  version_ = version;
}

void ManifestAssetManager::ComponentContext::SetUninstalling() {
  CHECK(state_ != ComponentState::kRegistering &&
        state_ != ComponentState::kUninstalling);
  state_ = ComponentState::kUninstalling;
  requested_version_ = kUninstallingVersion;
  install_dir_ = std::nullopt;
  version_ = std::nullopt;
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
  return context;
}

base::DictValue ManifestAssetManager::ComponentContext::ToValue() const {
  base::DictValue dict;
  dict.Set(kAssetIdKey, asset_id_);
  dict.Set(kRequestedVersionKey, requested_version_);
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

void ManifestAssetManager::AssetLedger::RemoveContext(
    const std::string& public_key) {
  component_contexts_.erase(public_key);
  ScopedDictPrefUpdate update(
      &*local_state_, model_execution::prefs::localstate::kManifestAssetLedger);
  update->Remove(public_key);
}

ManifestAssetManager::DiskSpaceStatus::DiskSpaceStatus() = default;
ManifestAssetManager::DiskSpaceStatus::~DiskSpaceStatus() = default;

void ManifestAssetManager::DiskSpaceStatus::Update(
    std::optional<base::ByteCount> free_space) {
  free_space_ = free_space;
  last_evaluated_ = base::Time::Now();
}

bool ManifestAssetManager::DiskSpaceStatus::IsFresh() const {
  return free_space_.has_value() &&
         (base::Time::Now() - last_evaluated_) < kDiskSpaceFreshnessThreshold;
}

bool ManifestAssetManager::DiskSpaceStatus::CanSupportOnDemandInstall() const {
  return free_space_.has_value() &&
         features::IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
             free_space_.value());
}

bool ManifestAssetManager::DiskSpaceStatus::CanSupportProactiveDownload()
    const {
  if (!base::FeatureList::IsEnabled(
          features::kOnDeviceModelBackgroundDownload)) {
    return false;
  }
  if (!base::PowerMonitor::GetInstance()->IsInitialized() ||
      base::PowerMonitor::GetInstance()->IsOnBatteryPower()) {
    return false;
  }
  return free_space_.has_value() &&
         features::IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
             free_space_.value());
}

ManifestAssetManager::ManifestAssetManager(
    PrefService& local_state,
    UsageTracker& usage_tracker,
    Delegate& delegate,
    component_updater::ComponentUpdateService* component_update_service,
    std::unique_ptr<ManifestSolutionFactory> factory)
    : usage_tracker_(usage_tracker),
      delegate_(delegate),
      component_update_service_(component_update_service),
      ledger_(local_state) {
  // Load persistent state from the ledger immediately on startup.
  ledger_.Load();

  // Update with the initial factory. This will call UpdateRegistration.
  UpdateSolutionFactory(std::move(factory));

  // Register observers for when we might need to update the registrations.
  usage_tracker_observation_.Observe(&usage_tracker);
}

ManifestAssetManager::~ManifestAssetManager() {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::~ManifestAssetManager",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ManifestAssetManager::AddDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!factory_) {
    return;
  }

  std::optional<absl::flat_hash_set<Manifest::AssetId>> required_assets =
      factory_->manifest().GetRequiredAssets(use_case);

  if (!required_assets) {
    return;
  }

  base::flat_set<std::string> component_ids;
  for (const auto& asset_id : *required_assets) {
    const auto& on_demand_components =
        factory_->manifest().GetAssets().on_demand_components();
    auto it = on_demand_components.find(asset_id);
    if (it != on_demand_components.end()) {
      std::vector<uint8_t> public_key_hash;
      if (base::HexStringToBytes(it->second.public_key(), &public_key_hash)) {
        component_ids.insert(
            crx_file::id_util::GenerateIdFromHash(public_key_hash));
      }
    }
  }

  auto& progress_manager = progress_managers_[use_case];
  if (!progress_manager) {
    progress_manager = std::make_unique<OnDeviceModelDownloadProgressManager>(
        component_update_service_, std::move(component_ids));
  }
  progress_manager->AddObserver(std::move(observer));
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

  // Mark the manifest asset as ready. This is deferred until now let the
  // AssetManager decide when the factory can start providing solutions.
  factory_->UpdateAssetState(kManifestAssetName,
                             ManifestSolutionFactory::AssetInfo{
                                 .path = factory_->manifest().GetDirectory()});

  // Make sure the ledger tracks all assets in the new manifest.
  for (const auto& [asset_id, component] :
       factory_->manifest().GetAssets().on_demand_components()) {
    ComponentContext& context =
        *ledger_.GetOrCreateContext(component.public_key());
    context.SetAssetId(asset_id);
  }

  std::vector<Manifest::UseCaseName> background_download_use_cases;
  for (const auto& [use_case_name, use_case_config] :
       factory_->manifest().GetDeviceCategoryConfig().use_cases()) {
    if (use_case_config.background_download()) {
      background_download_use_cases.push_back(use_case_name);
    }
  }
  background_download_assets_by_id_ =
      factory_->manifest().GetRequiredAssets(background_download_use_cases);

  UpdateActiveAssets();
}

void ManifestAssetManager::RefreshSolutions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (factory_) {
    factory_->UpdateSolutions();
  }
}

// static
bool ManifestAssetManager::VerifyInstallation(const base::FilePath& install_dir,
                                              const base::DictValue& manifest) {
  // TODO(crbug.com/489511499): implement proper verification logic.
  return base::PathExists(install_dir);
}

void ManifestAssetManager::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name,
    bool is_first_usage) {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::OnDeviceEligibleUseCaseUsed",
              perfetto::Flow::FromPointer(this), "use_case_name", use_case_name,
              "is_first_usage", is_first_usage);
  if (is_first_usage) {
    UpdateActiveAssets();
  }
}

// Get all assets required by used use cases in usage_tracker.
void ManifestAssetManager::UpdateActiveAssets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<Manifest::UseCaseName> active_use_cases;
  for (const auto& [use_case, _] :
       factory_->manifest().GetDeviceCategoryConfig().use_cases()) {
    if (usage_tracker_->WasUseCaseRecentlyUsed(use_case)) {
      active_use_cases.push_back(use_case);
    }
  }
  active_assets_by_id_ =
      factory_->manifest().GetRequiredAssets(active_use_cases);
  UpdateRegistrations();
}

void ManifestAssetManager::OnDiskSpaceEvaluated(
    std::optional<base::ByteCount> free_space) {
  TRACE_EVENT("optimization_guide",
              "ManifestAssetManager::OnDiskSpaceEvaluated",
              perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_space_status_.Update(free_space);
  UpdateRegistrations();
}

bool ManifestAssetManager::ShouldInstall(
    const ComponentContext& context,
    const proto::OnDemandComponent* component) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(disk_space_status_.IsFresh());
  if (!component) {
    return false;
  }
  if (context.requested_version() == component->target_version()) {
    // The component is either downloading or already installed.
    return true;
  }
  if (!disk_space_status_.CanSupportOnDemandInstall()) {
    std::optional<base::ByteCount> free_space =
        disk_space_status_.GetFreeSpace();
    if (free_space) {
      base::UmaHistogramCounts100(
          "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
          "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
          free_space->InGiB());
    }
    return false;
  }
  if (active_assets_by_id_.contains(context.asset_id())) {
    return true;
  }
  return disk_space_status_.CanSupportProactiveDownload() &&
         background_download_assets_by_id_.contains(context.asset_id());
}

void ManifestAssetManager::UpdateRegistrations() {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::UpdateRegistrations",
              perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!disk_space_status_.IsFresh()) {
    // Don't do anything yet, just refresh the disk space status.
    delegate_->GetFreeDiskSpace(
        base::BindOnce(&ManifestAssetManager::OnDiskSpaceEvaluated,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

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
    if (!ShouldInstall(context, component)) {
      if (context.NeedsCleanup()) {
        // Component is obsolete.
        context.SetUninstalling();

        Manifest::UninstallReason log_reason =
            Manifest::UninstallReason::kUnknown;
        log_reason = factory_->manifest().uninstall_reason();

        base::UmaHistogramEnumeration(
            "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason." +
                ConvertComponentKeyToUmaModelName(context.asset_id()),
            log_reason);

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
          public_key, component->target_version(), context.asset_id(),
          weak_ptr_factory_.GetWeakPtr());
      continue;
    }
    if (context.state() == ComponentState::kRegistered) {
      if (active_assets_by_id_.contains(context.asset_id())) {
        context.SetOnDemandDownloading();
        // This doesn't change a persistent state, so it's okay to not save.
        delegate_->RequestUpdate(public_key,
                                 /*is_background=*/false);
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
  delegate_->Uninstall(public_key, weak_ptr_factory_.GetWeakPtr());
}

void ManifestAssetManager::OnAssetUninstalled(const std::string& public_key) {
  TRACE_EVENT("optimization_guide", "ManifestAssetManager::OnAssetUninstalled",
              perfetto::Flow::FromPointer(this), "public_key", public_key);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const proto::OnDemandComponent* component =
      factory_->manifest().GetAssetByPublicKey(public_key);
  if (component) {
    // We've finished an uninstall, but the manifest might need this component.
    // Keep tracking it in the ledger, and check if we should install it again.
    ledger_.GetContext(public_key)->SetUninstallComplete();
    ledger_.SaveContexts({public_key});
    UpdateRegistrations();
    return;
  }
  // The component is not in the manifest or on disk, so we can remove it from
  // the ledger.
  ledger_.RemoveContext(public_key);
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
  bool is_new_installation =
      (context->state() == ComponentState::kRegistered ||
       context->state() == ComponentState::kOnDemandDownloading);
  context->SetReady(install_dir, version);

  OnDeviceBaseModel model_enum = ConvertComponentKeyToEnum(context->asset_id());
  base::UmaHistogramEnumeration(
      "OptimizationGuide.OnDeviceModel.InstalledModel", model_enum);
  if (is_new_installation) {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.OnDeviceModel.NewModelInstalled", model_enum);
  }

  NotifyFactory(public_key, *context);
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

std::vector<mojom::BrokerPropertyInfoPtr>
ManifestAssetManager::GetBrokerProperties() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<mojom::BrokerPropertyInfoPtr> properties;
  properties.push_back(mojom::BrokerPropertyInfo::New(
      "Supports Installs",
      disk_space_status_.CanSupportOnDemandInstall() ? "true" : "false"));
  properties.push_back(mojom::BrokerPropertyInfo::New(
      "Supports Proactive Downloads",
      disk_space_status_.CanSupportProactiveDownload() ? "true" : "false"));
  return properties;
}

std::vector<mojom::BrokerAssetInfoPtr> ManifestAssetManager::GetBrokerAssets()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<mojom::BrokerAssetInfoPtr> assets;
  for (const auto& [public_key, context] : ledger_.contexts()) {
    const proto::OnDemandComponent* component =
        factory_->manifest().GetAssetByPublicKey(public_key);
    assets.push_back(context.ToBrokerAssetInfo(component));
  }
  return assets;
}

std::vector<mojom::BrokerModelInfoPtr> ManifestAssetManager::GetBrokerModels()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return factory_->GetBrokerModels();
}

}  // namespace optimization_guide
