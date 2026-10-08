// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/model_execution/component_download_observer.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_monitor.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_solution_factory.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"

class PrefService;

namespace optimization_guide {

// Controls whether installed assets with `AssetPriority::kEvictable` are
// uninstalled.
BASE_DECLARE_FEATURE(kOnDeviceModelEviction);

// Controls whether to scan for orphaned assets on disk after initialization.
BASE_DECLARE_FEATURE(kScanForOrphanedManifestAssets);

// Delay after initialization before scanning for orphaned assets on disk.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kOrphanedManifestAssetScanDelay);

class UsageTracker;

// Priorities for assets in the manifest.
enum class AssetPriority {
  kEvictable = 0,
  kRetain = 1,
  kSpeculative = 2,
  kBestEffort = 3,
  kUserBlocking = 4,
};

// Tracks the priority of assets in the manifest.
class AssetPriorities {
 public:
  AssetPriorities();
  ~AssetPriorities();
  AssetPriorities(const AssetPriorities&);
  AssetPriorities& operator=(const AssetPriorities&);
  AssetPriorities(AssetPriorities&&);
  AssetPriorities& operator=(AssetPriorities&&);

  void Raise(AssetPriority priority,
             const absl::flat_hash_set<Manifest::AssetId>& assets);
  void Clear();

  bool IsAtLeast(AssetPriority priority,
                 const Manifest::AssetId& asset_id) const;

 private:
  absl::flat_hash_map<Manifest::AssetId, AssetPriority> priorities_;
};

// Manages the state of assets defined in the on-device model manifest.
class ManifestAssetManager : public UsageTracker::Observer,
                             public ManifestAssetManagerDelegate::AssetManager {
 public:
  // Constructs a ManifestAssetManager, and begins provide assets to the given
  // `factory`.
  explicit ManifestAssetManager(
      PrefService& local_state,
      UsageTracker& usage_tracker,
      ManifestAssetManagerDelegate& delegate,
      component_updater::ComponentUpdateService* component_update_service,
      std::unique_ptr<ManifestSolutionFactory> factory);
  ~ManifestAssetManager() override;

  ManifestAssetManager(const ManifestAssetManager&) = delete;
  ManifestAssetManager& operator=(const ManifestAssetManager&) = delete;

  // Add download progress observer for the given use case.
  void AddDownloadProgressObserver(
      const std::string& use_case,
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer);

  // Add download progress observer for the given asset.
  void AddAssetDownloadObserver(
      const std::string& asset_name,
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer);

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

  // Returns a list of assets for the broker state info.
  std::vector<mojom::BrokerAssetInfoPtr> GetBrokerAssets() const;

  // Returns a list of models for the broker state info.
  std::vector<std::pair<mojom::BrokerModelInfoPtr, base::FilePath>>
  GetBrokerModels() const;

  // ManifestAssetManagerDelegate::AssetManager:
  void OnAssetReady(const std::string& public_key,
                    const base::Version& version,
                    const base::FilePath& install_dir) override;
  void OnAssetUninstalled(const std::string& public_key) override;
  void InstallerRegistered(const std::string& public_key,
                           const std::string& version,
                           bool is_already_installed) override;

  // Recomputes the priorities of assets required by use cases.
  void RecomputeAssetPriorities();

  // Forces eviction of evictable assets to be enabled regardless of the
  // `kOnDeviceModelEviction` feature flag.
  void EnableEviction();

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
    // Resets the context after the component was uninstalled, whether by this
    // manager or by something else (e.g. the component updater unregistered
    // it). A registration in flight is left to complete.
    void SetUninstallComplete();
    void SetRegistering(const std::string& target_version);
    void SetRegistered();
    void SetOnDemandDownloading();
    void SetReadySoon();
    void SetReady(const base::FilePath& install_dir,
                  const base::Version& version);
    void SetUninstalling();
    void SetOrphaned(const std::optional<base::Version>& version);

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
  void OnPriorityIncrease(const std::string& use_case_name,
                          UsageTracker::Priority previous_priority) override;

  // Scans disk for installed assets not tracked in the ledger.
  void ScanForOrphanedAssets();
  void OnOrphanedAssetsFound(
      std::vector<ManifestAssetManagerDelegate::InstalledAsset>
          installed_assets);

  // Helper to resolve an asset name to its CRX ID.
  std::optional<std::string> GetCrxIdForAsset(
      const std::string& asset_name) const;

  // Get disk space, and call `UpdateRegistration` when done.
  void OnDiskSpaceEvaluated(std::optional<base::ByteSize> free_space);

  // Policy predicates governing component installation, retention, and
  // foreground updates.
  bool CanSupportProactiveDownload() const;
  // Assuming the component is already registered for (or has installed) the
  // target version, returns whether it should be retained rather than evicted.
  bool ShouldRetain(const ComponentContext& context) const;
  bool ShouldRegisterNewVersion(const ComponentContext& context) const;
  bool ShouldInstallOrRetain(const ComponentContext& context,
                             const proto::OnDemandComponent* component) const;
  bool ShouldRequestForegroundDownload(const ComponentContext& context) const;

  bool IsEvictionEnabled() const;

  void RecordEvictableAssetsCount() const;
  void RecordUninstallReason(const ComponentContext& context,
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

  const raw_ref<PrefService> local_state_;
  const raw_ref<UsageTracker> usage_tracker_;
  const raw_ref<ManifestAssetManagerDelegate> delegate_;
  raw_ptr<component_updater::ComponentUpdateService> component_update_service_;

  // Tracks the state of all components known to the manager. Keyed by the
  // component public key hex and computed for the union of components in the
  // manifest and persisted contexts.
  AssetLedger ledger_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unordered_map<std::string,
                     std::unique_ptr<OnDeviceModelDownloadProgressManager>>
      progress_managers_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<std::string, std::unique_ptr<ComponentDownloadObserver>>
      asset_download_observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the free disk space and the last time it was evaluated.
  struct DiskSpaceStatus {
    DiskSpaceStatus();
    ~DiskSpaceStatus();

    void Update(std::optional<base::ByteSize> free_space);

    bool IsFresh() const;
    bool IsSufficientForOnDemandInstall() const;
    bool IsSufficientForBackgroundInstall() const;
    std::optional<base::ByteSize> free_space() const { return free_space_; }

   private:
    std::optional<base::ByteSize> free_space_;
    base::Time last_evaluated_;
  };
  DiskSpaceStatus disk_space_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The solution factory to provide assets to. This provides the manifest.
  std::unique_ptr<ManifestSolutionFactory> factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  AssetPriorities asset_priorities_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool is_eviction_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::ScopedObservation<UsageTracker, UsageTracker::Observer>
      usage_tracker_observation_{this};
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ManifestAssetManager> weak_ptr_factory_{this};
};
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
