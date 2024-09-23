// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_URL_VISIT_RESUMPTION_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_URL_VISIT_RESUMPTION_RANKER_H_

#include <memory>

#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Ranker that provides score for tab resupmtion.
class URLVisitResumptionRanker : public DefaultModelProvider {
 public:
  explicit URLVisitResumptionRanker();
  ~URLVisitResumptionRanker() override;

  URLVisitResumptionRanker(const URLVisitResumptionRanker&) = delete;
  URLVisitResumptionRanker& operator=(const URLVisitResumptionRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;

 private:
  const bool use_random_score_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_URL_VISIT_RESUMPTION_RANKER_H_
