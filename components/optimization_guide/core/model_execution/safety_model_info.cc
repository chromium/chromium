
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/safety_model_info.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

namespace optimization_guide {

namespace {

class ScopedTextSafetyModelMetadataValidityLogger {
 public:
  ScopedTextSafetyModelMetadataValidityLogger() = default;
  ~ScopedTextSafetyModelMetadataValidityLogger() {
    CHECK_NE(TextSafetyModelMetadataValidity::kUnknown, validity_);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        validity_);
  }

  void set_validity(TextSafetyModelMetadataValidity validity) {
    validity_ = validity;
  }

  TextSafetyModelMetadataValidity validity_ =
      TextSafetyModelMetadataValidity::kUnknown;
};

bool HasRequiredSafetyFiles(SafetyModelInfo::SafetyModelType model_type,
                            const ModelInfo& model_info) {
  if (model_type == SafetyModelInfo::SafetyModelType::kGeneralizedSafetyModel) {
    // Generalized safety model does not require any additional file.
    return true;
  }

  return model_info.GetAdditionalFileWithBaseName(kTsDataFile) &&
         model_info.GetAdditionalFileWithBaseName(kTsSpModelFile);
}

}  // namespace

std::unique_ptr<SafetyModelInfo> SafetyModelInfo::Load(
    SafetyModelType model_type,
    base::optional_ref<const ModelInfo> opt_model_info) {
  if (!opt_model_info.has_value() ||
      !HasRequiredSafetyFiles(model_type, *opt_model_info)) {
    return nullptr;
  }
  const ModelInfo& model_info = *opt_model_info;
  ScopedTextSafetyModelMetadataValidityLogger logger;

  if (!model_info.GetModelMetadata()) {
    logger.set_validity(TextSafetyModelMetadataValidity::kNoMetadata);
    return nullptr;
  }

  std::optional<proto::TextSafetyModelMetadata> model_metadata =
      ParsedAnyMetadata<proto::TextSafetyModelMetadata>(
          *model_info.GetModelMetadata());
  if (!model_metadata) {
    logger.set_validity(TextSafetyModelMetadataValidity::kMetadataWrongType);
    return nullptr;
  }

  logger.set_validity(TextSafetyModelMetadataValidity::kNoFeatureConfigs);

  base::flat_map<proto::ModelExecutionFeature,
                 proto::FeatureTextSafetyConfiguration>
      feature_configs;
  for (const auto& feature_config :
       model_metadata->feature_text_safety_configurations()) {
    logger.set_validity(TextSafetyModelMetadataValidity::kValid);
    feature_configs[feature_config.feature()] = feature_config;
  }

  return base::WrapUnique(new SafetyModelInfo(
      model_type, model_info, model_metadata->num_output_categories(),
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

SafetyModelInfo::SafetyModelType SafetyModelInfo::GetModelType() const {
  return model_type_;
}

base::FilePath SafetyModelInfo::GetDataPath() const {
  if (model_type_ == SafetyModelType::kTextSafetyModel) {
    return *model_info_.GetAdditionalFileWithBaseName(kTsDataFile);
  } else {
    return model_info_.GetModelFilePath();
  }
}

base::FilePath SafetyModelInfo::GetSpModelPath() const {
  if (model_type_ == SafetyModelType::kGeneralizedSafetyModel) {
    return base::FilePath();
  }
  return *model_info_.GetAdditionalFileWithBaseName(kTsSpModelFile);
}

int64_t SafetyModelInfo::GetVersion() const {
  return model_info_.GetVersion();
}

SafetyModelInfo::SafetyModelInfo(
    SafetyModelType model_type,
    const ModelInfo& model_info,
    uint32_t num_output_categories,
    base::flat_map<proto::ModelExecutionFeature,
                   proto::FeatureTextSafetyConfiguration> feature_configs)
    : model_type_(model_type),
      model_info_(model_info),
      num_output_categories_(num_output_categories),
      feature_configs_(std::move(feature_configs)) {}

SafetyModelInfo::~SafetyModelInfo() = default;

}  // namespace optimization_guide
