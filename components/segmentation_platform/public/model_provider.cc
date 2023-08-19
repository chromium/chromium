// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

ModelProvider::ModelProvider(proto::SegmentId segment_id)
    : segment_id_(segment_id) {}

ModelProvider::~ModelProvider() = default;

ModelProviderFactory::~ModelProviderFactory() = default;

DefaultModelProvider::ModelConfig::ModelConfig(
    proto::SegmentationModelMetadata metadata,
    int64_t model_version)
    : metadata(std::move(metadata)), model_version(model_version) {}
DefaultModelProvider::ModelConfig::~ModelConfig() = default;

DefaultModelProvider::DefaultModelProvider(proto::SegmentId segment_id)
    : ModelProvider(segment_id) {}
DefaultModelProvider::~DefaultModelProvider() = default;

void DefaultModelProvider::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  std::unique_ptr<ModelConfig> config = GetModelConfig();
  model_updated_callback.Run(segment_id_, std::move(config->metadata),
                             config->model_version);
}

bool DefaultModelProvider::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
