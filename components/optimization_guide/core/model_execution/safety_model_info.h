
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_MODEL_INFO_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_MODEL_INFO_H_

#include <memory>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

namespace optimization_guide {

class SafetyModelInfo {
 public:
  ~SafetyModelInfo();

  static std::unique_ptr<SafetyModelInfo> Load(
      base::optional_ref<const ModelInfo> model_info);
  std::optional<proto::FeatureTextSafetyConfiguration> GetConfig(
      proto::ModelExecutionFeature feature) const;
  base::FilePath GetDataPath() const;
  base::FilePath GetSpModelPath() const;
  int64_t GetVersion() const;
  uint32_t num_output_categories() const { return num_output_categories_; }

 private:
  SafetyModelInfo(
      const ModelInfo& model_info,
      uint32_t num_output_categories,
      base::flat_map<proto::ModelExecutionFeature,
                     proto::FeatureTextSafetyConfiguration> feature_configs);

  const ModelInfo model_info_;
  const uint32_t num_output_categories_;
  base::flat_map<proto::ModelExecutionFeature,
                 proto::FeatureTextSafetyConfiguration>
      feature_configs_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_MODEL_INFO_H_
