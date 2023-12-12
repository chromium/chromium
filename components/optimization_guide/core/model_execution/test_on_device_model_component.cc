// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"

#include "base/check.h"

namespace optimization_guide {

class FakeOnDeviceModelComponentStateManagerDelegate
    : public OnDeviceModelComponentStateManager::Delegate {
 public:
  ~FakeOnDeviceModelComponentStateManagerDelegate() override = default;
  void RegisterInstaller(scoped_refptr<OnDeviceModelComponentStateManager>
                             state_manager) override {
    installer_registered_ = true;
  }
  void Uninstall(scoped_refptr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    uninstall_called_ = true;
  }

 private:
  friend class TestOnDeviceModelComponentStateManager;
  bool installer_registered_ = false;
  bool uninstall_called_ = false;
};

TestOnDeviceModelComponentStateManager::TestOnDeviceModelComponentStateManager(
    PrefService* local_state)
    : local_state_(local_state) {}

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
        std::make_unique<FakeOnDeviceModelComponentStateManagerDelegate>();
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
  }
}

bool TestOnDeviceModelComponentStateManager::IsInstallerRegistered() const {
  return delegate_ && delegate_->installer_registered_;
}

bool TestOnDeviceModelComponentStateManager::WasComponentUninstalled() const {
  return delegate_ && delegate_->uninstall_called_;
}

}  // namespace optimization_guide
