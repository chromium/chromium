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
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_monitor.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_solution_factory.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace optimization_guide {

class UsageTracker;
// Manages the state of assets defined in the on-device model manifest.
class ManifestAssetManager : public UsageTracker::Observer {
 public:
  // Delegate to bridge the gap to the platform-specific download mechanism
  // (e.g., Chrome Component Updater on Desktop, AICore on Android).
  class Delegate : public ManifestMonitor::Delegate {
   public:
    virtual ~Delegate() = default;

    // Registers the component installer for `public_key`. The policy should
    // hold a weak pointer to the manager and call its `OnAssetReady` and
    // `OnAssetUninstalled` methods when appropriate.
    virtual void RegisterOnDemandComponent(
        const std::string& public_key_hex,
        const std::string& target_version,
        const std::string& component_name,
        base::WeakPtr<ManifestAssetManager> manager) = 0;

    // Uninstalls a component and frees disk space.
    virtual void Uninstall(const std::string& public_key_hex,
                           base::WeakPtr<ManifestAssetManager> manager) = 0;

    // Triggers an immediate update check for a component.
    virtual void RequestUpdate(const std::string& public_key_hex,
                               bool is_background) = 0;
  };

  // Constructs a ManifestAssetManager, and begins provide assets to the given
  // `factory`.
  explicit ManifestAssetManager(
      PrefService& local_state,
      UsageTracker& usage_tracker,
      Delegate& delegate,
      std::unique_ptr<ManifestSolutionFactory> factory);
  ~ManifestAssetManager() override;

  ManifestAssetManager(const ManifestAssetManager&) = delete;
  ManifestAssetManager& operator=(const ManifestAssetManager&) = delete;

  // Tells the manager to begin providing assets to a new solution factory.
  // The `solution_factory` must not be null.
  // The asset manager will take the following actions in order, potentially
  // asynchronously.
  //  1. Stop any prior factory from providing new Solutions.  It *may* defer
  //     this action until some work completes.
  //  2. Provide the new factory with an initial state by for each asset
  //     via UpdateAssetState.
  void UpdateSolutionFactory(std::unique_ptr<ManifestSolutionFactory> factory);

  // Refreshes all solutions for the current factory, if any.
  void RefreshSolutions();

  // Returns a list of properties for the broker state info.
  std::vector<mojom::BrokerPropertyInfoPtr> GetBrokerProperties() const;

  // Returns a list of assets for the broker state info.
  std::vector<mojom::BrokerAssetInfoPtr> GetBrokerAssets() const;

  // Returns a list of models for the broker state info.
  std::vector<mojom::BrokerModelInfoPtr> GetBrokerModels() const;

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

  // Current state of a component.
  struct ComponentContext {
    ComponentContext();
    ~ComponentContext();
    ComponentContext(const ComponentContext&);
    ComponentContext& operator=(const ComponentContext&);

    // Prefs deserialization
    static ComponentContext FromValue(const base::DictValue& value);
    base::DictValue ToValue() const;

    // Whether there is data on disk to uninstall.
    bool NeedsCleanup() const;
    // Returns an AssetState for whether we have the target_version.
    ManifestSolutionFactory::AssetState AsAssetState(
        const std::string& target_version) const;

    std::string asset_id() const { return asset_id_; }
    std::string requested_version() const { return requested_version_; }
    ComponentState state() const { return state_; }
    std::optional<base::FilePath> install_dir() const { return install_dir_; }

    mojom::BrokerAssetState ToBrokerAssetState() const;
    mojom::BrokerAssetInfoPtr ToBrokerAssetInfo(
        const proto::OnDemandComponent* target) const;

    // Sets the ID used in the current manifest for this asset.
    void SetAssetId(const std::string& asset_id);

    // ComponentState transitions methods:
    void SetUninstallComplete();
    void SetRegistering(const std::string& target_version);
    void SetRegistered();
    void SetOnDemandDownloading();
    void SetReadySoon();
    void SetReady(const base::FilePath& install_dir,
                  const base::Version& version);
    void SetUninstalling();

   private:
    // Persistent state (saved to prefs)
    std::string asset_id_;  // Associated asset ID from manifest.
    std::string requested_version_;

    // Transient state (in memory)
    ComponentState state_ = ComponentState::kNotRegistered;
    std::optional<base::FilePath> install_dir_;
    std::optional<base::Version> version_;
  };

  // A ledger that handles saving and loading persistent component contexts to
  // prefs.
  class AssetLedger {
   public:
    explicit AssetLedger(PrefService& local_state);
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

    raw_ref<PrefService> local_state_;
    base::flat_map<std::string, ComponentContext> component_contexts_;
  };

  // UsageTracker::Observer:
  void OnDeviceEligibleUseCaseUsed(const std::string& use_case_name,
                                   bool is_first_usage) override;

  // Updates the set of assets required by the active use cases.
  void UpdateActiveAssets();

  // Get disk space, and call `UpdateRegistration` when done.
  void OnDiskSpaceEvaluated(std::optional<base::ByteCount> free_space);

  // Returns whether the asset should be installed.
  bool ShouldInstall(const ComponentContext& context,
                     const proto::OnDemandComponent* component) const;

  // Updates each component to move towards it's intended state.
  // May defer actions due to pending operations, like disk space evaluation
  // or outstanding registration requests.
  void UpdateRegistrations();

  // Begins the process of uninstalling the component with the given public key.
  void UninstallComponent(const std::string& public_key);

  // Notifies the factory about the current state of the component with the
  // given public key.
  void NotifyFactory(const std::string& public_key,
                     const ComponentContext& context);

  const raw_ref<UsageTracker> usage_tracker_;
  const raw_ref<Delegate> delegate_;

  // Tracks the state of all components known to the manager. Keyed by the
  // component public key hex and computed for the union of components in the
  // manifest and persisted contexts.
  AssetLedger ledger_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the free disk space and the last time it was evaluated.
  struct DiskSpaceStatus {
    DiskSpaceStatus();
    ~DiskSpaceStatus();

    void Update(std::optional<base::ByteCount> free_space);

    bool IsFresh() const;
    bool CanSupportOnDemandInstall() const;
    bool CanSupportProactiveDownload() const;
    std::optional<base::ByteCount> GetFreeSpace() const { return free_space_; }

   private:
    std::optional<base::ByteCount> free_space_;
    base::Time last_evaluated_;
  };
  DiskSpaceStatus disk_space_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The solution factory to provide assets to. This provides the manifest.
  std::unique_ptr<ManifestSolutionFactory> factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the manifest assets required by the active use cases.
  absl::flat_hash_set<Manifest::AssetId> active_assets_by_id_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the manifest assets that are enabled for background download.
  absl::flat_hash_set<Manifest::AssetId> background_download_assets_by_id_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<UsageTracker, UsageTracker::Observer>
      usage_tracker_observation_{this};
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ManifestAssetManager> weak_ptr_factory_{this};
};
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
