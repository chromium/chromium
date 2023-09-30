// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CROSS_DEVICE_USER_SEGMENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CROSS_DEVICE_USER_SEGMENT_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation Chrome cross device user model provider. Provides a default
// model and metadata for the cross device user optimization target.
class CrossDeviceUserSegment : public DefaultModelProvider {
 public:
  CrossDeviceUserSegment();
  ~CrossDeviceUserSegment() override = default;

  CrossDeviceUserSegment(const CrossDeviceUserSegment&) = delete;
  CrossDeviceUserSegment& operator=(const CrossDeviceUserSegment&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CROSS_DEVICE_USER_SEGMENT_H_
