// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

using on_device_model::mojom::LoadModelResult;

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  FakeOnDeviceModelServiceController(
      on_device_model::FakeOnDeviceServiceSettings* settings,
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  void LaunchService() override;

  void clear_did_launch_service() { did_launch_service_ = false; }

  bool did_launch_service() const { return did_launch_service_; }

  size_t on_device_model_receiver_count() const {
    return service_ ? service_->on_device_model_receiver_count() : 0;
  }

  void CrashService() { service_ = nullptr; }

 private:
  ~FakeOnDeviceModelServiceController() override;

  raw_ptr<on_device_model::FakeOnDeviceServiceSettings> settings_;
  std::unique_ptr<on_device_model::FakeOnDeviceModelService> service_;
  bool did_launch_service_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
