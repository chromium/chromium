// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_

#include <memory>
#include <string>

#include "base/byte_count.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

// Test stand-in for tracking what the ManifestAssetManager requests from the
// component updater via its Delegate.
class TestManifestAssetManagerComponentState final {
 public:
  // What the broker can request to be installed.
  struct InstallTarget {
    InstallTarget();
    InstallTarget(const std::string& public_key_hex,
                  const base::Version& version);
    ~InstallTarget();
    InstallTarget(const InstallTarget&);
    InstallTarget& operator=(const InstallTarget&);

    std::string public_key_hex;
    std::optional<base::Version> version;

    bool operator==(const InstallTarget& other) const {
      return public_key_hex == other.public_key_hex && version == other.version;
    }

    template <typename H>
    friend H AbslHashValue(H h, const InstallTarget& target) {
      return H::combine(std::move(h), target.public_key_hex,
                        target.version->components());
    }
  };

  // A component that we can simulate being installed.
  struct InstallableComponent {
    InstallableComponent();
    InstallableComponent(const InstallTarget& target,
                         const base::FilePath& install_dir);
    ~InstallableComponent();
    InstallableComponent(const InstallableComponent&);
    InstallableComponent& operator=(const InstallableComponent&);

    InstallTarget target;
    base::FilePath install_dir;
  };

  enum class DownloadScenario {
    kHealthy,    // All downloads will complete successfully.
    kThrottled,  // Only foreground downloads will complete.
    kOffline,    // No downloads will complete.
  };

  // Simulated Component Updater state for a single component.
  struct Registration {
    Registration();
    ~Registration();
    Registration(const Registration&);
    Registration& operator=(const Registration&);

    // The last target that was registered.
    InstallTarget target;
    // The manager to send callbacks to.
    base::WeakPtr<ManifestAssetManager> manager;
    // Whether the manager is expecting OnInstallerRegistered to be called.
    bool pending_registration = false;
    // Whether the manager is expecting OnAssetUninstalled to be called.
    bool pending_uninstall = false;
    // Whether a foreground OnDemandUpdate has been requested for this
    // registration.
    bool has_foreground_update_requested = false;
    // Whether a background OnDemandUpdate has been requested for this
    // registration.
    bool has_background_update_requested = false;
  };

  TestManifestAssetManagerComponentState();
  ~TestManifestAssetManagerComponentState();

  // Constructs the delegate for the ManifestBrokerState to use.
  std::unique_ptr<ManifestAssetManager::Delegate> CreateDelegate();

  /////////////////////////////
  // Behavior configuration  //
  /////////////////////////////

  void SetFreeDiskSpace(base::ByteCount free_space_bytes) {
    free_disk_space_ = free_space_bytes;
  }

  // If true, OnInstallerRegistered and OnAssetUninstalled will be deferred
  // instead of called immediately on registration/uninstallation.
  void SetDeferRegistrationCallbacks(bool defer) {
    defer_registration_callbacks_ = defer;
  }
  // Runs deferred callbacks for the given public key.
  void RunPendingRegistrations(const std::string& public_key);
  void RunPendingRegistrations(Registration& registration);
  // Runs deferred callbacks for all registrations.
  void RunPendingRegistrations();

  // Indicates how downloads should behave.
  void SetDownloadScenario(DownloadScenario scenario);

  // Methods for making components installable.
  // These will also cause installations to complete based on the download
  // scenario and active registrations.
  void UpdateManifest(std::unique_ptr<ManifestComponentDirectory> manifest_dir);
  void UpdateBaseModel(const std::string& public_key,
                       std::unique_ptr<FakeBaseModelAsset> asset);
  void UpdateModelAdaptation(const std::string& public_key,
                             std::unique_ptr<FakeAdaptationAsset> asset);
  void UpdateSafetyModel(const std::string& public_key,
                         std::unique_ptr<FakeSafetyModelAsset> asset);
  void UpdateLanguageDetectionModel(
      const std::string& public_key,
      std::unique_ptr<FakeLanguageModelAsset> asset);

  // Simulate restart behavior.
  // This will clear all registrations, but not installed components.
  void SimulateRestart();

  //////////////////////
  // Test assertions  //
  //////////////////////

  // Whether the component is registered with any target version.
  bool IsRegistered(const std::string& public_key) const;
  // Whether the component is registered with the given target version.
  bool IsRegistered(const InstallTarget& target) const;
  bool WasUninstallRequested(const std::string& public_key) const;
  bool WasOnDemandUpdateRequested(const std::string& public_key) const;
  bool WasBackgroundUpdateRequested(const std::string& public_key) const;

  bool WaitForRegistration(const InstallTarget& target) const {
    VLOG(1) << "WaitForRegistration PK:" << target.public_key_hex
            << " V:" << target.version->GetString();
    return base::test::RunUntil([&] { return IsRegistered(target); });
  }

  bool WaitForUninstall(const std::string& public_key) const {
    VLOG(1) << "WaitForUninstall PK:" << public_key;
    return base::test::RunUntil(
        [&]() { return WasUninstallRequested(public_key); });
  }

  bool IsInstalled(const InstallTarget& target) const;
  bool IsUninstalled(const std::string& public_key) const;

  // Provides access to the fake component update service for future
  // update simulation and progress tracking.
  component_updater::ComponentUpdateService& component_update_service() {
    return component_update_service_;
  }

 private:
  class DelegateImpl;

  void MaybeCompleteDownload(const std::string& public_key);

  // The simulated network behavior.
  DownloadScenario download_scenario_ = DownloadScenario::kHealthy;
  // The contents we are pretending that server has available for download.
  absl::flat_hash_map<InstallTarget, InstallableComponent>
      installable_components_;

  // The amount of free disk space we are pretending is available.
  base::ByteCount free_disk_space_ = base::GiB(30);
  // The directories that we are pretending that the component updater has
  // installed, keyed by public key.
  absl::flat_hash_map<std::string, InstallableComponent> installed_components_;
  // The path for the installed manifest.
  std::optional<base::FilePath> manifest_path_;

  // Whether to defer calling OnInstallerRegistered/OnAssetUninstalled.
  bool defer_registration_callbacks_ = false;
  // The simulated state of CUS registrations for on-demand components.
  absl::flat_hash_map<std::string, Registration> registrations_;

  // All registrations for the Manifest component.
  base::RepeatingCallbackList<void(base::FilePath)> manifest_ready_callbacks_;

  // Owned assets for simulations.
  std::vector<std::unique_ptr<ManifestComponentDirectory>> manifest_assets_;
  std::vector<std::unique_ptr<FakeBaseModelAsset>> base_model_assets_;
  std::vector<std::unique_ptr<FakeAdaptationAsset>> adaptation_assets_;
  std::vector<std::unique_ptr<FakeSafetyModelAsset>> safety_model_assets_;
  std::vector<std::unique_ptr<FakeLanguageModelAsset>> language_model_assets_;

  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  base::WeakPtrFactory<TestManifestAssetManagerComponentState>
      weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_
