// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace optimization_guide {

using OnDeviceFeatureSet = base::EnumSet<mojom::OnDeviceFeature,
                                         mojom::OnDeviceFeature::kMinValue,
                                         mojom::OnDeviceFeature::kMaxValue>;

// Return the name to use in histogram variants for this feature key.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetVariantName(mojom::OnDeviceFeature feature);

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

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_FEATURES_H_
