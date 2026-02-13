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
    state_manager->InstallerRegistered(state_ && state_->installed_asset_);
  }
  void Uninstall(base::WeakPtr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    if (state_) {
      state_->uninstall_called_ = true;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceModelComponentStateManager::UninstallComplete,
                       state_manager),
        base::Seconds(1));
  }
  void RequestUpdate(bool is_background) override {
    if (state_) {
      if (is_background) {
        state_->requested_background_update_ = true;
      } else {
        state_->requested_foreground_update_ = true;
      }
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

  std::string GetComponentId() override {
    return state_ ? state_->component().id() : std::string();
  }

 private:
  base::WeakPtr<TestComponentState> state_;
};

TestComponentState::TestComponentState() {
  component_updater::CrxUpdateItem item =
      component_.CreateUpdateItem(update_client::ComponentState::kNew, 0);

  ON_CALL(component_update_service_,
          GetComponentDetails(testing::_, testing::_))
      .WillByDefault(testing::DoAll(testing::SetArgPointee<1>(item),
                                    testing::Return(true)));
}

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

bool TestComponentState::WaitForDownloadObserver() const {
  return base::test::RunUntil(
      [&]() { return component_update_service_.HasObserver(); });
}

void TestComponentState::UpdateDownloadProgress(uint64_t downloaded_bytes) {
  component_update_service_.SendUpdate(component_.CreateUpdateItem(
      update_client::ComponentState::kDownloading, downloaded_bytes));
}

}  // namespace optimization_guide
