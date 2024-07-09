// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_

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

// Returns a validation config that passes with the default model settings.
inline proto::OnDeviceModelValidationConfig WillPassValidationConfig() {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  // This prompt passes because by default the model will echo the input.
  prompt->set_prompt("hElLo");
  prompt->set_expected_output("HeLlO");
  return validation_config;
}

// Returns a validation config that fails with the default model settings.
inline proto::OnDeviceModelValidationConfig WillFailValidationConfig() {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  // This prompt fails because by default the model will echo the input.
  prompt->set_prompt("hello");
  prompt->set_expected_output("goodbye");
  return validation_config;
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
