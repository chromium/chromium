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
  bool IsRegistered(const std::string& asset_id) const;
  bool WasUninstalled(const std::string& asset_id) const;
  bool WasOnDemandUpdateRequested(const std::string& asset_id) const;
  bool WasBackgroundUpdateRequested(const std::string& asset_id) const;

  // Simulates the component updater finishing a download/install
  void SimulateAssetReady(const std::string& asset_id,
                          const base::Version& version,
                          const base::FilePath& install_dir);

  // Provides access to the fake component update service for future
  // update simulation and progress tracking.
  component_updater::ComponentUpdateService& component_update_service() {
    return component_update_service_;
  }

 private:
  class DelegateImpl;

  base::ByteCount free_disk_space_ = base::GiB(100);

  base::flat_set<std::string> registered_assets_;
  base::flat_set<std::string> uninstalled_assets_;
  base::flat_set<std::string> foreground_updates_requested_;
  base::flat_set<std::string> background_updates_requested_;

  // Track the managers to simulate callbacks from the component updater.
  base::flat_map<std::string, base::WeakPtr<ManifestAssetManager>> managers_;

  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  base::WeakPtrFactory<TestManifestAssetManagerComponentState>
      weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_TEST_MANIFEST_ASSET_MANAGER_COMPONENT_STATE_H_
