// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"

#include <memory>
#include <string>

#include "base/byte_count.h"
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
      const std::string& public_key_hex,
      const std::string& target_version,
      base::WeakPtr<ManifestAssetManager> manager) override {
    bool is_already_installed = false;
    if (!state_) {
      // Test fixture destroyed, do nothing.
      return;
    }
    state_->registered_components_.insert(public_key_hex);
    state_->managers_[public_key_hex] = manager;
    is_already_installed =
        state_->already_installed_components_.contains(public_key_hex);
    if (state_->defer_registration_callbacks_) {
      state_->pending_registrations_.push_back(
          base::BindOnce(&ManifestAssetManager::InstallerRegistered, manager,
                         public_key_hex, target_version, is_already_installed));
      return;
    }
    manager->InstallerRegistered(public_key_hex, target_version,
                                 is_already_installed);
  }

  void Uninstall(const std::string& public_key_hex,
                 base::WeakPtr<ManifestAssetManager> manager) override {
    if (state_) {
      state_->uninstalled_components_.insert(public_key_hex);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ManifestAssetManager::OnAssetUninstalled, manager,
                       public_key_hex),
        base::Seconds(1));
  }

  void RequestUpdate(const std::string& public_key_hex,
                     bool is_background) override {
    if (state_) {
      if (is_background) {
        state_->background_updates_requested_.insert(public_key_hex);
      } else {
        state_->foreground_updates_requested_.insert(public_key_hex);
      }
    }
  }

  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) const override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       state_ ? state_->free_disk_space_ : base::ByteCount(0)));
  }

  base::FilePath GetInstallDirectory() const override {
    return base::FilePath(FILE_PATH_LITERAL("/tmp/manifest_asset_install_dir"));
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

void TestManifestAssetManagerComponentState::RunPendingRegistrations() {
  auto pending = std::move(pending_registrations_);
  pending_registrations_.clear();
  for (auto& cb : pending) {
    std::move(cb).Run();
  }
}

bool TestManifestAssetManagerComponentState::IsRegistered(
    const std::string& public_key_hex) const {
  return registered_components_.contains(public_key_hex);
}

bool TestManifestAssetManagerComponentState::WasUninstallRequested(
    const std::string& public_key_hex) const {
  return uninstalled_components_.contains(public_key_hex);
}

bool TestManifestAssetManagerComponentState::WasOnDemandUpdateRequested(
    const std::string& public_key_hex) const {
  return foreground_updates_requested_.contains(public_key_hex);
}

bool TestManifestAssetManagerComponentState::WasBackgroundUpdateRequested(
    const std::string& public_key_hex) const {
  return background_updates_requested_.contains(public_key_hex);
}

void TestManifestAssetManagerComponentState::SimulateComponentReady(
    const std::string& public_key_hex,
    const base::Version& version,
    const base::FilePath& install_dir) {
  auto it = managers_.find(public_key_hex);
  if (it != managers_.end() && it->second) {
    it->second->OnAssetReady(public_key_hex, version, install_dir);
  }
}

}  // namespace optimization_guide
