// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation low engagement model provider. Provides a default model and
// metadata for the low user engagement optimization target.
class LowUserEngagementModel : public DefaultModelProvider {
 public:
  LowUserEngagementModel();
  ~LowUserEngagementModel() override = default;

  // Disallow copy/assign.
  LowUserEngagementModel(const LowUserEngagementModel&) = delete;
  LowUserEngagementModel& operator=(const LowUserEngagementModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_
