// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_on_device_capability_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockOptimizationGuideOnDeviceCapabilityProvider
    : public OptimizationGuideOnDeviceCapabilityProvider {
 public:
  MockOptimizationGuideOnDeviceCapabilityProvider();
  MockOptimizationGuideOnDeviceCapabilityProvider(
      const MockOptimizationGuideOnDeviceCapabilityProvider&) = delete;
  MockOptimizationGuideOnDeviceCapabilityProvider& operator=(
      const MockOptimizationGuideOnDeviceCapabilityProvider&) = delete;
  ~MockOptimizationGuideOnDeviceCapabilityProvider() override;

  MOCK_METHOD(OnDeviceModelEligibilityReason,
              GetOnDeviceModelEligibility,
              (ModelBasedCapabilityKey),
              (override));

  MOCK_METHOD(void,
              GetOnDeviceModelEligibilityAsync,
              (ModelBasedCapabilityKey,
               const on_device_model::Capabilities&,
               base::OnceCallback<void(OnDeviceModelEligibilityReason)>),
              (override));

  MOCK_METHOD(std::optional<SamplingParamsConfig>,
              GetSamplingParamsConfig,
              (ModelBasedCapabilityKey),
              (override));

  MOCK_METHOD(std::optional<const optimization_guide::proto::Any>,
              GetFeatureMetadata,
              (optimization_guide::ModelBasedCapabilityKey feature),
              (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_ON_DEVICE_CAPABILITY_PROVIDER_H_
