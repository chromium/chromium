// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_VALIDATION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_VALIDATION_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace optimization_guide {

class ManifestValidator {
 public:
  ManifestValidator(OnDeviceModelAccessController& access_controller,
                    ModelBrokerImpl& model_broker);
  ~ManifestValidator();

  void MaybeExecuteValidationTask(const proto::ValidationTask& task);

 private:
  void StartValidation(base::WeakPtr<ModelClient> client);
  void OnClientAvailable(base::WeakPtr<ModelClient> client);
  void OnValidationComplete(OnDeviceModelValidationResult result);

  base::raw_ref<OnDeviceModelAccessController> access_controller_;
  base::raw_ref<ModelBrokerImpl> model_broker_;
  proto::ValidationTask task_;
  std::unique_ptr<OnDeviceModelValidator> validator_;
  base::WeakPtrFactory<ManifestValidator> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_VALIDATION_H_
