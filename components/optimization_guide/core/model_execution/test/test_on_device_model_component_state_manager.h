// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

class PrefService;

namespace base {
class FilePath;
}

namespace optimization_guide {

class FakeOnDeviceModelComponentStateManagerDelegate;

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

  void SetReady(const base::FilePath& install_dir,
                const std::string& version = "0.0.1");

  struct State;

 private:
  raw_ptr<PrefService> local_state_;
  raw_ptr<FakeOnDeviceModelComponentStateManagerDelegate> delegate_;
  scoped_refptr<OnDeviceModelComponentStateManager> manager_;
  scoped_refptr<State> state_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_TEST_ON_DEVICE_MODEL_COMPONENT_STATE_MANAGER_H_
