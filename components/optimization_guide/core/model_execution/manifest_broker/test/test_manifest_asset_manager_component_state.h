// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_

#include <memory>
#include <string>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

// Test stand-in for tracking what the ManifestAssetManager requests from the
// component updater via its Delegate.
class TestManifestAssetManagerComponentState final {
 public:
  TestManifestAssetManagerComponentState();
  ~TestManifestAssetManagerComponentState();

  std::unique_ptr<ManifestAssetManager::Delegate> CreateDelegate();

  // Test assertions
  bool IsRegistered(const std::string& public_key) const;
  bool WasUninstallRequested(const std::string& public_key) const;
  bool WasOnDemandUpdateRequested(const std::string& public_key) const;
  bool WasBackgroundUpdateRequested(const std::string& public_key) const;

  bool WaitForRegistration(const std::string& public_key) const {
    return base::test::RunUntil([&] { return IsRegistered(public_key); });
  }

  bool WaitForUninstall(const std::string& public_key) const {
    return base::test::RunUntil(
        [&]() { return WasUninstallRequested(public_key); });
  }

  // Test manipulators.
  void SetFreeDiskSpace(base::ByteCount free_space_bytes) {
    free_disk_space_ = free_space_bytes;
  }

  void SetDeferRegistrationCallbacks(bool defer) {
    defer_registration_callbacks_ = defer;
  }
  void RunPendingRegistrations();

  void ClearRegistered() { registered_components_.clear(); }
  // Simulates the component updater finishing a download/install
  void SimulateComponentReady(const std::string& public_key,
                              const base::Version& version,
                              const base::FilePath& install_dir);

  // Sets up a component to simulate being already installed when registered.
  void SetAlreadyInstalled(const std::string& public_key) {
    already_installed_components_.insert(public_key);
  }

  // Provides access to the fake component update service for future
  // update simulation and progress tracking.
  component_updater::ComponentUpdateService& component_update_service() {
    return component_update_service_;
  }

 private:
  class DelegateImpl;

  base::ByteCount free_disk_space_ = base::GiB(100);

  // Tracks requests from the ManifestAssetManager to the component updater,
  // keyed by public key.
  base::flat_set<std::string> registered_components_;
  base::flat_set<std::string> uninstalled_components_;
  base::flat_set<std::string> foreground_updates_requested_;
  base::flat_set<std::string> background_updates_requested_;
  base::flat_set<std::string> already_installed_components_;

  bool defer_registration_callbacks_ = false;
  std::vector<base::OnceClosure> pending_registrations_;

  // Track the managers to simulate callbacks from the component updater, keyed
  // by public key.
  base::flat_map<std::string, base::WeakPtr<ManifestAssetManager>> managers_;

  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  base::WeakPtrFactory<TestManifestAssetManagerComponentState>
      weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_
