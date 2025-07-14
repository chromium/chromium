// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

class PrefService;

namespace optimization_guide {

class FakeBaseModelAsset;

// Test stand in for component_updater infrastructure.
class TestComponentState final {
 public:
  TestComponentState();
  ~TestComponentState();

  std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
  CreateDelegate();

  void SetFreeDiskSpace(int64_t free_space_bytes) {
    free_disk_space_ = free_space_bytes;
  }
  bool installer_registered() const { return installer_registered_; }
  bool uninstall_called() const { return uninstall_called_; }

 private:
  class DelegateImpl;

  int64_t free_disk_space_ = 100 * 1024ll * 1024 * 1024;
  bool installer_registered_ = false;
  bool uninstall_called_ = false;
  base::WeakPtrFactory<TestComponentState> weak_ptr_factory_{this};
};

// Provides scoped creation and destruction of
// OnDeviceModelComponentStateManager. Checks to make sure only one instance is
// used at a time.
class TestOnDeviceModelComponentStateManager {
 public:
  explicit TestOnDeviceModelComponentStateManager(PrefService* local_state);
  ~TestOnDeviceModelComponentStateManager();

  scoped_refptr<OnDeviceModelComponentStateManager> get();

  void Reset();

  bool IsInstallerRegistered() const;
  bool WasComponentUninstalled() const;

  void SetFreeDiskSpace(int64_t free_space_bytes);

  void SetReady(const FakeBaseModelAsset& asset);

 private:
  raw_ptr<PrefService> local_state_;
  scoped_refptr<OnDeviceModelComponentStateManager> manager_;
  std::unique_ptr<TestComponentState> state_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
