// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_on_device_model_service_controller.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

FakeOnDeviceModelServiceController::FakeOnDeviceModelServiceController(
    on_device_model::FakeOnDeviceServiceSettings* settings,
    std::unique_ptr<OnDeviceModelAccessController> access_controller,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::move(access_controller),
          std::move(on_device_component_state_manager)),
      settings_(settings) {}

FakeOnDeviceModelServiceController::~FakeOnDeviceModelServiceController() =
    default;

void FakeOnDeviceModelServiceController::LaunchService() {
  if (service_remote_) {
    return;
  }
  did_launch_service_ = true;
  service_remote_.reset();
  service_ = std::make_unique<on_device_model::FakeOnDeviceModelService>(
      service_remote_.BindNewPipeAndPassReceiver(), settings_);
  service_remote_.reset_on_disconnect();
  service_remote_.reset_on_idle_timeout(base::TimeDelta());
}

}  // namespace optimization_guide
