// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

namespace optimization_guide {

using OnDeviceFeatureSet = base::EnumSet<mojom::OnDeviceFeature,
                                         mojom::OnDeviceFeature::kMinValue,
                                         mojom::OnDeviceFeature::kMaxValue>;

enum class OnDeviceModelType {
  kBaseModel,
  kClassifierModel,
};

// Return the name to use in histogram variants for this feature key.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string_view GetVariantName(mojom::OnDeviceFeature feature);

// Returns the model type required for the feature.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
OnDeviceModelType GetOnDeviceModelType(mojom::OnDeviceFeature feature);

// Returns which ModelExecutionFeature is used for this feature key.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    mojom::OnDeviceFeature feature);

// Returns the opt target to use for on-device configuration for `feature`.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
proto::OptimizationTarget GetOptimizationTargetForFeature(
    mojom::OnDeviceFeature feature);

// Returns which feature key is associated with this ModelExecutionFeature, or
// nullopt if none.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<mojom::OnDeviceFeature> ToOnDeviceFeature(
    proto::ModelExecutionFeature feature);

// Maps a feature to its corresponding use case name.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string ToUseCaseName(mojom::OnDeviceFeature feature);

// Returns the feature that maps to the given use case name.
// Returns std::nullopt if no feature maps to the use case.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<mojom::OnDeviceFeature> GetFeatureForUseCase(
    const std::string& use_case_name);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_
