
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/safety_model_info.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

namespace optimization_guide {

std::unique_ptr<SafetyModelInfo> SafetyModelInfo::Load(
    base::optional_ref<const ModelInfo> opt_model_info) {
  if (!opt_model_info.has_value()) {
    return nullptr;
  }
  const ModelInfo& model_info = *opt_model_info;

  if (!model_info.model_metadata) {
    return nullptr;
  }

  std::optional<proto::TextSafetyModelMetadata> model_metadata =
      ParsedAnyMetadata<proto::TextSafetyModelMetadata>(
          *model_info.model_metadata);
  if (!model_metadata) {
    return nullptr;
  }

  base::flat_map<proto::ModelExecutionFeature,
                 proto::FeatureTextSafetyConfiguration>
      feature_configs;
  for (const auto& feature_config :
       model_metadata->feature_text_safety_configurations()) {
    feature_configs[feature_config.feature()] = feature_config;
  }

  return base::WrapUnique(
      new SafetyModelInfo(model_info, model_metadata->num_output_categories(),
                          std::move(feature_configs)));
}

std::optional<proto::FeatureTextSafetyConfiguration> SafetyModelInfo::GetConfig(
    proto::ModelExecutionFeature feature) const {
  auto it = feature_configs_.find(feature);
  if (it == feature_configs_.end()) {
    return std::nullopt;
  }

  return it->second;
}

base::FilePath SafetyModelInfo::GetDataPath() const {
  return model_info_.model_file_path;
}

int64_t SafetyModelInfo::GetVersion() const {
  return model_info_.version;
}

SafetyModelInfo::SafetyModelInfo(
    const ModelInfo& model_info,
    uint32_t num_output_categories,
    base::flat_map<proto::ModelExecutionFeature,
                   proto::FeatureTextSafetyConfiguration> feature_configs)
    : model_info_(model_info),
      num_output_categories_(num_output_categories),
      feature_configs_(std::move(feature_configs)) {}

SafetyModelInfo::~SafetyModelInfo() = default;

}  // namespace optimization_guide
