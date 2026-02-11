// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_

#include <memory>

#include "base/byte_count.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/current_thread.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"

namespace optimization_guide {

class FakeBaseModelAsset;

// Test stand in for component_updater infrastructure.
class TestComponentState final {
 public:
  TestComponentState();
  ~TestComponentState();

  std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
  CreateDelegate();

  void SetFreeDiskSpace(base::ByteCount free_space_bytes) {
    free_disk_space_ = free_space_bytes;
  }
  bool installer_registered() const { return !!registered_manager_; }
  bool uninstall_called() const { return uninstall_called_; }
  bool requested_background_update() const {
    return requested_background_update_;
  }
  bool requested_foreground_update() const {
    return requested_foreground_update_;
  }

  void Install(std::unique_ptr<FakeBaseModelAsset> asset);
  void SimulateShutdown() {
    registered_manager_.reset();
    uninstall_called_ = false;
  }

  bool WaitForRegistration() const {
    return base::test::RunUntil([&]() { return installer_registered(); });
  }

  bool WaitForDownloadObserver() const;

  void UpdateDownloadProgress(uint64_t downloaded_bytes);

  FakeComponent& component() { return component_; }
  component_updater::ComponentUpdateService& component_update_service() {
    return component_update_service_;
  }

 private:
  class DelegateImpl;

  base::ByteCount free_disk_space_ = base::GiB(100);
  base::WeakPtr<OnDeviceModelComponentStateManager> registered_manager_;
  bool uninstall_called_ = false;
  bool requested_background_update_ = false;
  bool requested_foreground_update_ = false;
  std::unique_ptr<FakeBaseModelAsset> installed_asset_;
  FakeComponent component_{"component_id", 1000};
  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  base::WeakPtrFactory<TestComponentState> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
