// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to predict whether the user belongs to IosModuleRanker segment.
class IosModuleRanker : public DefaultModelProvider {
 public:
  IosModuleRanker();
  ~IosModuleRanker() override = default;

  IosModuleRanker(const IosModuleRanker&) = delete;
  IosModuleRanker& operator=(const IosModuleRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

// Test model for IosModuleRanker that uses same config, but just gives the card
// passed through the '--test-ios-module-ranker' commandline arg a top score.
class TestIosModuleRanker : public DefaultModelProvider {
 public:
  TestIosModuleRanker();
  ~TestIosModuleRanker() override = default;

  TestIosModuleRanker(const TestIosModuleRanker&) = delete;
  TestIosModuleRanker& operator=(const TestIosModuleRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_
