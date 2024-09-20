// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_METRICS_CLUSTERING_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_METRICS_CLUSTERING_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to predict whether the user belongs to MetricsClustering segment.
class MetricsClustering : public DefaultModelProvider {
 public:
  static constexpr char kMetricsClusteringKey[] = "metrics_clustering";
  static constexpr char kMetricsClusteringUmaName[] = "MetricsClustering";

  MetricsClustering();
  ~MetricsClustering() override = default;

  MetricsClustering(const MetricsClustering&) = delete;
  MetricsClustering& operator=(const MetricsClustering&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_METRICS_CLUSTERING_H_
