// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TAB_RESUMPTION_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TAB_RESUMPTION_RANKER_H_

#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"
namespace segmentation_platform {

// Ranker that provides score for tab resupmtion.
class TabResumptionRanker : public DefaultModelProvider {
 public:
  TabResumptionRanker();
  ~TabResumptionRanker() override;

  TabResumptionRanker(const TabResumptionRanker&) = delete;
  TabResumptionRanker& operator=(const TabResumptionRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;

 private:
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TAB_RESUMPTION_RANKER_H_
