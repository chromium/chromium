// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"

#include "base/byte_count.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

namespace optimization_guide {

class TestComponentState::DelegateImpl
    : public OnDeviceModelComponentStateManager::Delegate {
 public:
  explicit DelegateImpl(base::WeakPtr<TestComponentState> state)
      : state_(state) {}
  ~DelegateImpl() override = default;

  // OnDeviceModelComponentStateManager::Delegate.
  void RegisterInstaller(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
      OnDeviceModelRegistrationAttributes attributes) override {
    if (state_) {
      state_->registered_manager_ = state_manager;
      if (state_->installed_asset_) {
        state_->installed_asset_->SetReadyIn(*state_manager);
      }
    }
    state_manager->InstallerRegistered();
  }
  void Uninstall(base::WeakPtr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    if (state_) {
      state_->uninstall_called_ = true;
    }
  }
  base::FilePath GetInstallDirectory() override {
    return base::FilePath(FILE_PATH_LITERAL("/tmp/model_install_dir"));
  }
  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) override {
    base::ByteCount space =
        state_ ? state_->free_disk_space_ : base::ByteCount(0);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), space));
  }

 private:
  base::WeakPtr<TestComponentState> state_;
};

TestComponentState::TestComponentState() = default;
TestComponentState::~TestComponentState() = default;

std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
TestComponentState::CreateDelegate() {
  return std::make_unique<DelegateImpl>(weak_ptr_factory_.GetWeakPtr());
}

void TestComponentState::Install(std::unique_ptr<FakeBaseModelAsset> asset) {
  installed_asset_ = std::move(asset);
  if (registered_manager_) {
    installed_asset_->SetReadyIn(*registered_manager_);
  }
}

}  // namespace optimization_guide
