// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_ANDROID_HOME_MODULE_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_ANDROID_HOME_MODULE_RANKER_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to predict whether the user belongs to AndroidHomeModuleRanker segment.
class AndroidHomeModuleRanker : public DefaultModelProvider {
 public:
  AndroidHomeModuleRanker();
  ~AndroidHomeModuleRanker() override = default;

  AndroidHomeModuleRanker(const AndroidHomeModuleRanker&) = delete;
  AndroidHomeModuleRanker& operator=(const AndroidHomeModuleRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;

  bool is_android_home_module_ranker_v2_enabled{false};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_ANDROID_HOME_MODULE_RANKER_H_
