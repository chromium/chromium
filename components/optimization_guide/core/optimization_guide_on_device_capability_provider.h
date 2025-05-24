// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_

#include <optional>

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "services/on_device_model/public/cpp/capabilities.h"

namespace optimization_guide {

// Provides capability information about the on-device models to be served by
// the Optimization Guide.
class OptimizationGuideOnDeviceCapabilityProvider {
 public:
  // TODO(crbug.com/372349624): we could consider extending the response to
  // provide the factory of `OptimizationGuideModelExecutor`.
  virtual OnDeviceModelEligibilityReason GetOnDeviceModelEligibility(
      ModelBasedCapabilityKey feature) = 0;
  // Similar to above, but bumps the priority of related tasks such as computing
  // the performance class before returning the eligibility.
  virtual void GetOnDeviceModelEligibilityAsync(
      ModelBasedCapabilityKey feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback) = 0;
  virtual std::optional<SamplingParamsConfig> GetSamplingParamsConfig(
      ModelBasedCapabilityKey feature) = 0;
  virtual std::optional<const optimization_guide::proto::Any>
  GetFeatureMetadata(optimization_guide::ModelBasedCapabilityKey feature) = 0;

 protected:
  OptimizationGuideOnDeviceCapabilityProvider() = default;
  virtual ~OptimizationGuideOnDeviceCapabilityProvider() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_
