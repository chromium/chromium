// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_util.h"

#include "base/files/file_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

model_execution::prefs::GenAILocalFoundationalModelEnterprisePolicySettings
GetGenAILocalFoundationalModelEnterprisePolicySettings(
    PrefService* local_state) {
  return static_cast<model_execution::prefs::
                         GenAILocalFoundationalModelEnterprisePolicySettings>(
      local_state->GetInteger(
          model_execution::prefs::localstate::
              kGenAILocalFoundationalModelEnterprisePolicySettings));
}

std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& config_path) {
  // Unpack and verify model config file.
  std::string binary_config_pb;
  if (!base::ReadFileToString(config_path, &binary_config_pb)) {
    return nullptr;
  }

  auto config = std::make_unique<proto::OnDeviceModelExecutionConfig>();
  if (!config->ParseFromString(binary_config_pb)) {
    return nullptr;
  }
  return config;
}

bool WasOnDeviceEligibleFeatureRecentlyUsed(ModelBasedCapabilityKey feature,
                                            const PrefService& local_state) {
  if (!features::internal::GetOptimizationTargetForCapability(feature)) {
    return false;
  }
  return model_execution::prefs::WasFeatureRecentlyUsed(&local_state, feature);
}

}  // namespace optimization_guide
