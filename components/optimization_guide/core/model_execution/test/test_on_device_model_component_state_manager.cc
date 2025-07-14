// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
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
      scoped_refptr<OnDeviceModelComponentStateManager> state_manager,
      bool is_already_installing) override {
    if (state_) {
      state_->installer_registered_ = true;
    }
  }
  void Uninstall(scoped_refptr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    if (state_) {
      state_->uninstall_called_ = true;
    }
  }
  base::FilePath GetInstallDirectory() override {
    return base::FilePath(FILE_PATH_LITERAL("/tmp/model_install_dir"));
  }
  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(int64_t)> callback) override {
    int64_t space = state_ ? state_->free_disk_space_ : 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), space));
  }

 private:
  friend class TestOnDeviceModelComponentStateManager;
  base::WeakPtr<TestComponentState> state_;
};

TestComponentState::TestComponentState() = default;
TestComponentState::~TestComponentState() = default;

std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
TestComponentState::CreateDelegate() {
  return std::make_unique<DelegateImpl>(weak_ptr_factory_.GetWeakPtr());
}

TestOnDeviceModelComponentStateManager::TestOnDeviceModelComponentStateManager(
    PrefService* local_state)
    : local_state_(local_state),
      state_(std::make_unique<TestComponentState>()) {}

TestOnDeviceModelComponentStateManager::
    ~TestOnDeviceModelComponentStateManager() {
  Reset();
}

scoped_refptr<OnDeviceModelComponentStateManager>
TestOnDeviceModelComponentStateManager::get() {
  // Note that we create the instance lazily to allow tests time to register
  // prefs.
  if (!manager_) {
    CHECK(!OnDeviceModelComponentStateManager::GetInstanceForTesting())
        << "Instance already exists";
    manager_ = OnDeviceModelComponentStateManager::CreateOrGet(
        local_state_, state_->CreateDelegate());
    CHECK(manager_);
  }
  return manager_;
}

void TestOnDeviceModelComponentStateManager::Reset() {
  manager_ = nullptr;
  state_ = std::make_unique<TestComponentState>();
}

bool TestOnDeviceModelComponentStateManager::IsInstallerRegistered() const {
  return state_->installer_registered();
}

bool TestOnDeviceModelComponentStateManager::WasComponentUninstalled() const {
  return state_->uninstall_called();
}

void TestOnDeviceModelComponentStateManager::SetFreeDiskSpace(
    int64_t free_space_bytes) {
  state_->SetFreeDiskSpace(free_space_bytes);
}

void TestOnDeviceModelComponentStateManager::SetReady(
    const FakeBaseModelAsset& asset) {
  asset.SetReadyIn(*get());
}

}  // namespace optimization_guide
