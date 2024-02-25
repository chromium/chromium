// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation Chrome Power user model provider. Provides a default model and
// metadata for the Power user optimization target.
class PowerUserSegment : public DefaultModelProvider {
 public:
  PowerUserSegment();
  ~PowerUserSegment() override = default;

  PowerUserSegment(const PowerUserSegment&) = delete;
  PowerUserSegment& operator=(const PowerUserSegment&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_
