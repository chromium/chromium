// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"

namespace optimization_guide {

struct TestOnDeviceModelComponentStateManager::State
    : public base::RefCounted<TestOnDeviceModelComponentStateManager::State> {
  int64_t free_disk_space_ = 100 * 1024ll * 1024 * 1024;
  bool installer_registered_ = false;
  bool uninstall_called_ = false;

 private:
  friend class base::RefCounted<State>;
  ~State() = default;
};

class FakeOnDeviceModelComponentStateManagerDelegate
    : public OnDeviceModelComponentStateManager::Delegate {
 public:
  explicit FakeOnDeviceModelComponentStateManagerDelegate(
      scoped_refptr<TestOnDeviceModelComponentStateManager::State> state)
      : state_(state) {}
  ~FakeOnDeviceModelComponentStateManagerDelegate() override = default;

  // OnDeviceModelComponentStateManager::Delegate.
  void RegisterInstaller(scoped_refptr<OnDeviceModelComponentStateManager>
                             state_manager) override {
    state_->installer_registered_ = true;
  }
  void Uninstall(scoped_refptr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    state_->uninstall_called_ = true;
  }
  base::FilePath GetInstallDirectory() override {
    return base::FilePath(FILE_PATH_LITERAL("/tmp/model_install_dir"));
  }
  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(int64_t)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), state_->free_disk_space_));
  }

 private:
  friend class TestOnDeviceModelComponentStateManager;
  scoped_refptr<TestOnDeviceModelComponentStateManager::State> state_;
};

TestOnDeviceModelComponentStateManager::TestOnDeviceModelComponentStateManager(
    PrefService* local_state)
    : local_state_(local_state), state_(base::MakeRefCounted<State>()) {}

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
    auto delegate =
        std::make_unique<FakeOnDeviceModelComponentStateManagerDelegate>(
            state_);
    delegate_ = delegate.get();
    manager_ = OnDeviceModelComponentStateManager::CreateOrGet(
        local_state_, std::move(delegate));
    CHECK(manager_);
  }
  return manager_;
}

void TestOnDeviceModelComponentStateManager::Reset() {
  if (manager_) {
    delegate_ = nullptr;
    manager_ = nullptr;
    state_ = base::MakeRefCounted<State>();
  }
}

bool TestOnDeviceModelComponentStateManager::IsInstallerRegistered() const {
  return state_->installer_registered_;
}

bool TestOnDeviceModelComponentStateManager::WasComponentUninstalled() const {
  return state_->uninstall_called_;
}

void TestOnDeviceModelComponentStateManager::SetFreeDiskSpace(
    int64_t free_space_bytes) {
  state_->free_disk_space_ = free_space_bytes;
}

void TestOnDeviceModelComponentStateManager::SetReady(
    const base::FilePath& install_dir,
    const std::string& version) {
  auto manifest = base::Value::Dict().Set(
      "BaseModelSpec",
      base::Value::Dict().Set("version", "0.0.1").Set("name", "Test"));
  get()->SetReady(base::Version(version), install_dir, manifest);
}

}  // namespace optimization_guide
