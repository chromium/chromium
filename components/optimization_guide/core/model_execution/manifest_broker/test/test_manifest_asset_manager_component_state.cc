// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class TestManifestAssetManagerComponentState::DelegateImpl
    : public ManifestAssetManager::Delegate {
 public:
  explicit DelegateImpl(
      base::WeakPtr<TestManifestAssetManagerComponentState> state)
      : state_(state) {}
  ~DelegateImpl() override = default;

  void RegisterOnDemandComponent(
      const std::string& asset_id,
      const std::string& public_key_hex,
      const std::string& target_version,
      base::WeakPtr<ManifestAssetManager> manager) override {
    if (state_) {
      state_->registered_assets_.insert(asset_id);
      state_->managers_[asset_id] = manager;
    }
    manager->InstallerRegistered(asset_id, /*is_already_installed=*/false);
  }

  void Uninstall(const std::string& asset_id,
                 const std::string& public_key_hex,
                 base::WeakPtr<ManifestAssetManager> manager) override {
    if (state_) {
      state_->uninstalled_assets_.insert(asset_id);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ManifestAssetManager::OnAssetUninstalled, manager,
                       asset_id),
        base::Seconds(1));
  }

  void RequestUpdate(const std::string& asset_id,
                     const std::string& public_key_hex,
                     bool is_background) override {
    if (state_) {
      if (is_background) {
        state_->background_updates_requested_.insert(asset_id);
      } else {
        state_->foreground_updates_requested_.insert(asset_id);
      }
    }
  }

 private:
  base::WeakPtr<TestManifestAssetManagerComponentState> state_;
};

TestManifestAssetManagerComponentState::
    TestManifestAssetManagerComponentState() = default;
TestManifestAssetManagerComponentState::
    ~TestManifestAssetManagerComponentState() = default;

std::unique_ptr<ManifestAssetManager::Delegate>
TestManifestAssetManagerComponentState::CreateDelegate() {
  return std::make_unique<DelegateImpl>(weak_ptr_factory_.GetWeakPtr());
}

bool TestManifestAssetManagerComponentState::IsRegistered(
    const std::string& asset_id) const {
  return registered_assets_.contains(asset_id);
}

bool TestManifestAssetManagerComponentState::WasUninstalled(
    const std::string& asset_id) const {
  return uninstalled_assets_.contains(asset_id);
}

bool TestManifestAssetManagerComponentState::WasOnDemandUpdateRequested(
    const std::string& asset_id) const {
  return foreground_updates_requested_.contains(asset_id);
}

bool TestManifestAssetManagerComponentState::WasBackgroundUpdateRequested(
    const std::string& asset_id) const {
  return background_updates_requested_.contains(asset_id);
}

void TestManifestAssetManagerComponentState::SimulateAssetReady(
    const std::string& asset_id,
    const base::Version& version,
    const base::FilePath& install_dir) {
  auto it = managers_.find(asset_id);
  if (it != managers_.end() && it->second) {
    it->second->OnAssetReady(asset_id, version, install_dir);
  }
}

}  // namespace optimization_guide
