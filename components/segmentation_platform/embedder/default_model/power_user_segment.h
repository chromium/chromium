// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation Chrome Power user model provider. Provides a default model and
// metadata for the Power user optimization target.
class PowerUserSegment : public ModelProvider {
 public:
  PowerUserSegment();
  ~PowerUserSegment() override = default;

  PowerUserSegment(const PowerUserSegment&) = delete;
  PowerUserSegment& operator=(const PowerUserSegment&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // Returns the name of the subsegment for the given segment and the
  // `subsegment_rank`. The `subsegment_rank` should be computed based on the
  // subsegment discrete mapping in the model metadata.
  static absl::optional<std::string> GetSubsegmentName(int subsegment_rank);

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_POWER_USER_SEGMENT_H_
