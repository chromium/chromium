// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace optimization_guide {

class UsageTracker;
// Manages the state of assets defined in the on-device model manifest.
class ManifestAssetManager : public UsageTracker::Observer {
 public:
  // Delegate to bridge the gap to the platform-specific download mechanism
  // (e.g., Chrome Component Updater on Desktop, AICore on Android).
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Registers the component installer for `public_key`. The policy should
    // hold a weak pointer to the manager and call its `OnAssetReady` and
    // `OnAssetUninstalled` methods when appropriate.
    virtual void RegisterOnDemandComponent(
        const std::string& public_key_hex,
        const std::string& target_version,
        base::WeakPtr<ManifestAssetManager> manager) = 0;

    // Uninstalls a component and frees disk space.
    virtual void Uninstall(const std::string& public_key_hex,
                           base::WeakPtr<ManifestAssetManager> manager) = 0;

    // Triggers an immediate update check for a component.
    virtual void RequestUpdate(const std::string& public_key_hex,
                               bool is_background) = 0;

    // Gets the available free disk space on a background thread.
    virtual void GetFreeDiskSpace(
        const base::FilePath& path,
        base::OnceCallback<void(std::optional<base::ByteCount>)> callback)
        const = 0;

    // Returns the base install directory for on-demand models.
    virtual base::FilePath GetInstallDirectory() const = 0;
  };

  explicit ManifestAssetManager(PrefService* local_state,
                                UsageTracker& usage_tracker,
                                std::unique_ptr<Delegate> delegate,
                                Manifest manifest);
  ~ManifestAssetManager() override;

  ManifestAssetManager(const ManifestAssetManager&) = delete;
  ManifestAssetManager& operator=(const ManifestAssetManager&) = delete;

  // Updates a new manifest and update registration for all eligible assets.
  void UpdateManifest(Manifest manifest);

  // Returns the installation directory for `asset_id` if it is currently ready.
  std::optional<base::FilePath> GetInstallDirectory(
      const std::string& asset_id) const;

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::DictValue& manifest);

  // Called when a component has been successfully installed or updated.
  void OnAssetReady(const std::string& public_key,
                    const base::Version& version,
                    const base::FilePath& install_dir);

  // Called when a component has been completely uninstalled.
  void OnAssetUninstalled(const std::string& public_key);

  // Called when the component installer has finished registering the asset.
  void InstallerRegistered(const std::string& public_key,
                           const std::string& version,
                           bool is_already_installed);

 private:
  enum class ComponentState {
    // Asset is not registered with the component updater.
    kNotRegistered,
    // Delegate->RegisterOnDemandComponent called, waiting for callback.
    kRegistering,
    // Component registered, sitting idle.
    kRegistered,
    // Delegate->RequestUpdate called, actively downloading.
    kOnDemandDownloading,
    // Component is fully downloaded and verified.
    kReady,
    // Delegate->Uninstall called, waiting for completion.
    kUninstalling,
  };

  enum class InstallMode {
    // Install the model with on-demand install (foreground download).
    kOnDemand = 0,
    // Install the model by registering the component and wait for regular
    // schedule.
    kRegisterOnly = 1,
    kMaxValue = kRegisterOnly,
  };

  // Requirements that apply to all components for installation.
  struct GlobalRegistrationCriteria {
    bool enabled_by_feature = false;
    bool enabled_by_enterprise_policy = false;
    bool enabled_by_user_setting = false;
    // A recent enough disk space evaluation.
    base::ByteCount disk_space_free;

    bool IsDiskSpaceAvailable() const {
      return features::IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
          disk_space_free);
    }

    // TODO(crbug.com/489511499): Migrate this check to ManifestMonitor.
    bool IsRunningOutOfDiskSpace() const {
      return features::IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
          disk_space_free);
    }

    bool IsModelAllowed() const {
      return enabled_by_feature && enabled_by_enterprise_policy &&
             enabled_by_user_setting;
    }
  };

  // Component-specific criteria for installation.
  struct ComponentRegistrationCriteria {
    // True if the asset is part of the recipe for a requested use case.
    bool required_by_active_use_case = false;
    // True if the asset is already installing or registered with the component
    // updater previously, and the installation may or may not be complete.
    bool is_already_installing = false;
    // True if the asset is not in the current manifest.
    bool is_obsolete = false;
    // True if the asset's last registration timestamp is no longer within the
    // retention period.
    bool out_of_retention = false;

    bool ShouldUninstall(
        const GlobalRegistrationCriteria& global_criteria) const {
      return is_already_installing &&
             (is_obsolete || out_of_retention ||
              global_criteria.IsRunningOutOfDiskSpace() ||
              !global_criteria.IsModelAllowed());
    }

    std::optional<InstallMode> GetInstallMode(
        const GlobalRegistrationCriteria& global_criteria) const {
      if (ShouldUninstall(global_criteria) ||
          !global_criteria.IsDiskSpaceAvailable() ||
          !global_criteria.IsModelAllowed()) {
        return std::nullopt;
      }
      if (required_by_active_use_case) {
        return InstallMode::kOnDemand;
      }
      // TODO(crbug.com/489511499): Support registerOnly (proactive download)
      // for some solutions.
      return std::nullopt;
    }
  };

  // Current state of a component.
  struct ComponentContext {
    ComponentContext();
    ~ComponentContext();
    ComponentContext(const ComponentContext&);
    ComponentContext& operator=(const ComponentContext&);

    // Persistent state (saved to prefs)
    std::string asset_id;  // Associated asset ID from manifest.
    std::string requested_version;
    base::Time last_eligible_time = base::Time::Min();

    // Transient state (in memory)
    ComponentState state = ComponentState::kNotRegistered;
    std::optional<base::FilePath> install_dir;
    std::optional<base::Version> version;
  };

  // A ledger that handles saving and loading persistent component contexts to
  // prefs.
  class AssetLedger {
   public:
    explicit AssetLedger(PrefService* local_state);
    ~AssetLedger();

    // Loads all persistent contexts from prefs, used in initialization.
    void Load();

    const base::flat_map<std::string, ComponentContext>& contexts() const {
      return component_contexts_;
    }
    base::flat_map<std::string, ComponentContext>& GetMutableContexts() {
      return component_contexts_;
    }

    const ComponentContext* GetContext(const std::string& public_key) const;
    ComponentContext* GetContext(const std::string& public_key);
    ComponentContext* GetOrCreateContext(const std::string& public_key);

    // Flush the contexts for the given public keys to prefs.
    void SaveContexts(const std::vector<std::string>& public_keys);
    // Removes the context for the given public key from the in memory map and
    // prefs.
    void RemoveContext(const std::string& public_key);

   private:
    ComponentContext* GetContextImpl(const std::string& public_key,
                                     bool create_if_missing);

    raw_ptr<PrefService> local_state_;
    base::flat_map<std::string, ComponentContext> component_contexts_;
  };

  // UsageTracker::Observer:
  void OnDeviceEligibleUseCaseUsed(const std::string& use_case_name,
                                   bool is_first_usage) override;

  // Observes pref changes in policy and settings.
  void OnGenAILocalFoundationalModelEnterprisePolicyChanged();
  void OnGenAILocalFoundationalModelUserSettingChanged();

  // Get disk space, and call `UpdateRegistration` when done.
  void OnDiskSpaceEvaluated(std::optional<base::ByteCount> free_space);

  // Evaluates registration criteria and updates component states, possibly
  // deferring until disk space evaluation finishes.
  void UpdateRegistration();
  absl::flat_hash_set<Manifest::AssetId> GetActiveAssets() const;

  // Computes and updates the records for components in the union of the
  // manifest and persisted contexts.
  // `active_assets` are the assets required by the active use cases.
  void ComputeAndUpdateComponentContexts(
      const absl::flat_hash_set<Manifest::AssetId>& active_assets);

  // Enforces the computed registration criteria by updating component states.
  void EnforceRegistration();

  void UninstallComponent(const std::string& public_key);

  GlobalRegistrationCriteria ComputeGlobalRegistrationCriteria(
      std::optional<base::ByteCount> disk_space_free) const;
  ComponentRegistrationCriteria ComputeComponentRegistrationCriteria(
      const ComponentContext& context,
      bool required_by_active_use_case,
      bool is_obsolete) const;

  std::unique_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  Manifest manifest_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::raw_ref<PrefService> local_state_;
  PrefChangeRegistrar pref_change_registrar_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const base::raw_ref<UsageTracker> usage_tracker_;
  base::ScopedObservation<UsageTracker, UsageTracker::Observer>
      usage_tracker_observation_{this};

  // Tracks the state of all components known to the manager. Keyed by the
  // component public key hex and computed for the union of components in the
  // manifest and persisted contexts.
  AssetLedger ledger_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the registration criteria for all components. Keyed by the
  // component public key hex and computed for the union of components in the
  // manifest and persisted contexts.
  base::flat_map<std::string, ComponentRegistrationCriteria>
      component_registration_criteria_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Unset until first registration attempt.
  std::optional<GlobalRegistrationCriteria> global_criteria_
      GUARDED_BY_CONTEXT(sequence_checker_);

  struct DiskSpaceStatus {
    std::optional<base::ByteCount> free_space;
    base::TimeTicks last_evaluated;
  };
  DiskSpaceStatus disk_space_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ManifestAssetManager> weak_ptr_factory_{this};
};
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
