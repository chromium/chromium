// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_

#include <memory>

#include "base/check.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

// Returns the GenAILocalFoundationalModelEnterprisePolicySettings from the
// `local_state`.
model_execution::prefs::GenAILocalFoundationalModelEnterprisePolicySettings
GetGenAILocalFoundationalModelEnterprisePolicySettings(
    PrefService* local_state);

// Returns the model execution config read from the `config_path`.
std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& config_path);

// Returns whether the `feature` was recently used.
bool WasOnDeviceEligibleFeatureRecentlyUsed(ModelBasedCapabilityKey feature,
                                            const PrefService& local_state);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
