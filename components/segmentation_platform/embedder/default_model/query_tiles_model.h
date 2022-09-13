// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Segmentation query tiles model provider. Provides a default model and
// metadata for the query tiles optimization target.
class QueryTilesModel : public ModelProvider {
 public:
  QueryTilesModel();
  ~QueryTilesModel() override = default;

  // Disallow copy/assign.
  QueryTilesModel(QueryTilesModel&) = delete;
  QueryTilesModel& operator=(QueryTilesModel&) = delete;

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_
