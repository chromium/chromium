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
#include "base/notimplemented.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
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
constexpr char kLastEligibleTimeKey[] = "last_eligible_time";
}  // namespace

ManifestAssetManager::ComponentContext::ComponentContext() = default;
ManifestAssetManager::ComponentContext::~ComponentContext() = default;
ManifestAssetManager::ComponentContext::ComponentContext(
    const ComponentContext&) = default;
ManifestAssetManager::ComponentContext&
ManifestAssetManager::ComponentContext::operator=(const ComponentContext&) =
    default;

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
    ComponentContext context;
    if (const std::string* id = entry_dict->FindString(kAssetIdKey)) {
      context.asset_id = *id;
    }
    if (const std::string* v = entry_dict->FindString(kRequestedVersionKey)) {
      context.requested_version = *v;
    }
    if (const base::Value* t = entry_dict->Find(kLastEligibleTimeKey)) {
      context.last_eligible_time =
          base::ValueToTime(*t).value_or(base::Time::Min());
    }
    component_contexts_.emplace(public_key, std::move(context));
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
    base::DictValue entry_dict;
    entry_dict.Set(kAssetIdKey, context->asset_id);
    entry_dict.Set(kRequestedVersionKey, context->requested_version);
    entry_dict.Set(kLastEligibleTimeKey,
                   base::TimeToValue(context->last_eligible_time));
    update->Set(public_key, std::move(entry_dict));
  }
}

void ManifestAssetManager::AssetLedger::RemoveContext(
    const std::string& public_key) {
  component_contexts_.erase(public_key);
  ScopedDictPrefUpdate update(
      &*local_state_, model_execution::prefs::localstate::kManifestAssetLedger);
  update->Remove(public_key);
}

ManifestAssetManager::ManifestAssetManager(PrefService& local_state,
                                           UsageTracker& usage_tracker,
                                           Delegate& delegate,
                                           Manifest manifest)
    : local_state_(local_state),
      usage_tracker_(usage_tracker),
      delegate_(delegate),
      manifest_(std::move(manifest)),
      ledger_(local_state) {
  // Load persistent state from the ledger immediately on startup.
  ledger_.Load();

  usage_tracker_observation_.Observe(&usage_tracker);

  pref_change_registrar_.Init(&local_state_.get());
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      base::BindRepeating(
          &ManifestAssetManager::
              OnGenAILocalFoundationalModelEnterprisePolicyChanged,
          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      base::BindRepeating(&ManifestAssetManager::
                              OnGenAILocalFoundationalModelUserSettingChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  model_execution::prefs::PruneOldUsagePrefs(&local_state_.get());
  // Start registration immediately because performance class computation is
  // already complete when the manager is created.
  UpdateRegistration();
}

ManifestAssetManager::~ManifestAssetManager() = default;

void ManifestAssetManager::UpdateManifest(Manifest manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manifest_ = std::move(manifest);
  UpdateRegistration();
}

std::optional<base::FilePath> ManifestAssetManager::GetInstallDirectory(
    const std::string& asset_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto component_it =
      manifest_.GetAssets().on_demand_components().find(asset_id);
  if (component_it == manifest_.GetAssets().on_demand_components().end()) {
    // Asset not found in current manifest.
    return std::nullopt;
  }

  const ComponentContext* context =
      ledger_.GetContext(component_it->second.public_key());
  if (context && context->install_dir) {
    return context->install_dir;
  }
  return std::nullopt;
}

// static
bool ManifestAssetManager::VerifyInstallation(const base::FilePath& install_dir,
                                              const base::DictValue& manifest) {
  // TODO(crbug.com/489511499): implement proper verification logic.
  return base::PathExists(install_dir);
}

void ManifestAssetManager::OnDiskSpaceEvaluated(
    std::optional<base::ByteCount> free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_space_status_.free_space = free_space;
  disk_space_status_.last_evaluated = base::TimeTicks::Now();
  UpdateRegistration();
}

void ManifestAssetManager::UpdateRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Query disk space again if it's stale. Otherwise use the cached value.
  bool is_fresh = !disk_space_status_.last_evaluated.is_null() &&
                  (base::TimeTicks::Now() - disk_space_status_.last_evaluated <
                   kDiskSpaceFreshnessThreshold);
  if (!is_fresh) {
    delegate_->GetFreeDiskSpace(
        base::BindOnce(&ManifestAssetManager::OnDiskSpaceEvaluated,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  global_criteria_ =
      ComputeGlobalRegistrationCriteria(disk_space_status_.free_space);
  ComputeAndUpdateComponentContexts(GetActiveAssets());
  EnforceRegistration();
}

// Get all assets required by used use cases in usage_tracker.
absl::flat_hash_set<Manifest::AssetId> ManifestAssetManager::GetActiveAssets()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<Manifest::UseCaseName> active_use_cases;
  for (const auto& [use_case, _] :
       manifest_.GetDeviceCategoryConfig().use_cases()) {
    if (usage_tracker_->WasUseCaseRecentlyUsed(use_case)) {
      active_use_cases.push_back(use_case);
    }
  }
  return manifest_.GetRequiredAssets(active_use_cases);
}

void ManifestAssetManager::ComputeAndUpdateComponentContexts(
    const absl::flat_hash_set<Manifest::AssetId>& active_assets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Compute registration for the union of all components in the manifest and
  // those in persistent contexts, and save the contexts that we intend to
  // register into the ledger.
  component_registration_criteria_.clear();

  // 1. Build a lookup map of public_key -> Component for current manifest
  // assets.
  absl::flat_hash_map<std::string, const proto::OnDemandComponent*>
      manifest_map;
  for (const auto& [asset_id, component] :
       manifest_.GetAssets().on_demand_components()) {
    manifest_map[component.public_key()] = &component;
    ledger_.GetOrCreateContext(component.public_key())->asset_id = asset_id;
  }

  std::vector<std::string> keys_to_save;
  // 2. Iterate over all tracked components to compute criteria.
  for (auto& [public_key, context] : ledger_.GetMutableContexts()) {
    auto it = manifest_map.find(public_key);
    bool is_in_manifest = (it != manifest_map.end());

    if (is_in_manifest) {
      const auto& component = *it->second;
      if (context.requested_version != component.target_version() &&
          context.state != ComponentState::kRegistering &&
          context.state != ComponentState::kUninstalling) {
        // Re-register if target version has changed. If the component is
        // currently registering or uninstalling, handle in callbacks.
        context.state = ComponentState::kNotRegistered;
      }
      context.requested_version = component.target_version();
    }

    const ComponentRegistrationCriteria criteria =
        ComputeComponentRegistrationCriteria(
            context,
            /*required_by_active_use_case=*/
            active_assets.contains(context.asset_id),
            /*is_obsolete=*/!is_in_manifest);

    if (criteria.GetInstallMode(*global_criteria_).has_value()) {
      context.last_eligible_time = base::Time::Now();
      keys_to_save.push_back(public_key);
    }
    component_registration_criteria_[public_key] = criteria;
  }

  if (!keys_to_save.empty()) {
    ledger_.SaveContexts(keys_to_save);
  }
}

void ManifestAssetManager::EnforceRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_criteria_);

  for (auto& [public_key, context] : ledger_.GetMutableContexts()) {
    if (context.state == ComponentState::kRegistering ||
        context.state == ComponentState::kUninstalling) {
      // Can't do anything right now during
      // registering/uninstalling, wait for callbacks.
      continue;
    }

    auto criteria_it = component_registration_criteria_.find(public_key);
    if (criteria_it == component_registration_criteria_.end()) {
      // Shouldn't happen since criteria are computed for all components in the
      // manifest plus persisted contexts.
      LOG(ERROR) << "No criteria found for component: " << public_key;
      continue;
    }
    const ComponentRegistrationCriteria& criteria = criteria_it->second;

    if (criteria.ShouldUninstall(*global_criteria_)) {
      context.state = ComponentState::kUninstalling;
      // Uninstall the component which will delete the model files, after a
      // short delay to give time for the consumers to unload the model.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ManifestAssetManager::UninstallComponent,
                         weak_ptr_factory_.GetWeakPtr(), public_key),
          kUninstallDelay);
      continue;
    }

    std::optional<InstallMode> mode =
        criteria.GetInstallMode(*global_criteria_);
    if (context.state == ComponentState::kNotRegistered) {
      if (mode.has_value() || criteria.is_already_installing) {
        context.state = ComponentState::kRegistering;
        delegate_->RegisterOnDemandComponent(public_key,
                                             context.requested_version,
                                             weak_ptr_factory_.GetWeakPtr());
      }
      continue;
    }

    if (context.state == ComponentState::kRegistered) {
      if (mode.has_value() && *mode == InstallMode::kOnDemand) {
        context.state = ComponentState::kOnDemandDownloading;
        delegate_->RequestUpdate(public_key,
                                 /*is_background=*/false);
      }
      continue;
    }
    CHECK(context.state == ComponentState::kOnDemandDownloading ||
          context.state == ComponentState::kReady);
  }
}

void ManifestAssetManager::UninstallComponent(const std::string& public_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->Uninstall(public_key, weak_ptr_factory_.GetWeakPtr());
}

ManifestAssetManager::GlobalRegistrationCriteria
ManifestAssetManager::ComputeGlobalRegistrationCriteria(
    std::optional<base::ByteCount> disk_space_free) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GlobalRegistrationCriteria result;
  result.enabled_by_feature = features::IsOnDeviceExecutionEnabled();
  result.enabled_by_enterprise_policy =
      GetGenAILocalFoundationalModelEnterprisePolicySettings(
          &local_state_.get()) ==
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
  result.enabled_by_user_setting = local_state_->GetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled);
  result.disk_space_free = disk_space_free.value_or(base::ByteCount(-1));

  return result;
}

ManifestAssetManager::ComponentRegistrationCriteria
ManifestAssetManager::ComputeComponentRegistrationCriteria(
    const ComponentContext& context,
    bool required_by_active_use_case,
    bool is_obsolete) const {
  ComponentRegistrationCriteria criteria;
  criteria.required_by_active_use_case = required_by_active_use_case;
  criteria.is_already_installing =
      context.last_eligible_time != base::Time::Min();
  criteria.is_obsolete = is_obsolete;
  criteria.out_of_retention = (base::Time::Now() - context.last_eligible_time) >
                              features::GetOnDeviceModelRetentionTime();
  return criteria;
}

void ManifestAssetManager::InstallerRegistered(const std::string& public_key,
                                               const std::string& version,
                                               bool is_already_installed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentContext* context = ledger_.GetContext(public_key);
  if (!context) {
    LOG(ERROR) << "Installer registered for unknown public key: " << public_key;
    return;
  }
  if (context->requested_version != version) {
    // Register again if target version changed while registering.
    context->state = ComponentState::kNotRegistered;
  } else {
    context->state = is_already_installed ? ComponentState::kReady
                                          : ComponentState::kRegistered;
  }
  EnforceRegistration();
}

void ManifestAssetManager::OnAssetReady(const std::string& public_key,
                                        const base::Version& version,
                                        const base::FilePath& install_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentContext* context = ledger_.GetContext(public_key);
  if (!context) {
    LOG(ERROR) << "Asset ready for unknown public key: " << public_key;
    return;
  }
  context->state = ComponentState::kReady;
  context->install_dir = install_dir;
  context->version = version;

  // TODO(crbug.com/489511499): notify `SolutionFactory` using context.asset_id.
}

void ManifestAssetManager::OnAssetUninstalled(const std::string& public_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  component_registration_criteria_.erase(public_key);
  ledger_.RemoveContext(public_key);
  // TODO(crbug.com/489511499): notify consumers.
}

void ManifestAssetManager::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name,
    bool is_first_usage) {
  UpdateRegistration();
}

void ManifestAssetManager::
    OnGenAILocalFoundationalModelEnterprisePolicyChanged() {
  UpdateRegistration();
}

void ManifestAssetManager::OnGenAILocalFoundationalModelUserSettingChanged() {
  UpdateRegistration();
}

}  // namespace optimization_guide
