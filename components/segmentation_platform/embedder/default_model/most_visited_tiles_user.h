// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_MOST_VISITED_TILES_USER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_MOST_VISITED_TILES_USER_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Feature flag for enabling MostVisitedTilesUser segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformMostVisitedTilesUser);

// Model to predict whether the user belongs to MostVisitedTilesUser segment.
class MostVisitedTilesUser : public DefaultModelProvider {
 public:
  static constexpr char kMostVisitedTilesUserKey[] = "most_visited_tiles_user";
  static constexpr char kMostVisitedTilesUserUmaName[] = "MostVisitedTilesUser";

  MostVisitedTilesUser();
  ~MostVisitedTilesUser() override = default;

  MostVisitedTilesUser(const MostVisitedTilesUser&) = delete;
  MostVisitedTilesUser& operator=(const MostVisitedTilesUser&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_MOST_VISITED_TILES_USER_H_
