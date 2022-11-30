// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/mock_model_provider.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/contains.h"

namespace segmentation_platform {

namespace {

using ::testing::_;
using ::testing::Invoke;

// Stores the client callbacks to |data|.
void StoreClientCallback(
    proto::SegmentId segment_id,
    TestModelProviderFactory::Data* data,
    const ModelProvider::ModelUpdatedCallback& model_updated_callback) {
  data->model_providers_callbacks.emplace(
      std::make_pair(segment_id, model_updated_callback));
}

}  // namespace

MockModelProvider::MockModelProvider(
    proto::SegmentId segment_id,
    base::RepeatingCallback<void(const ModelProvider::ModelUpdatedCallback&)>
        get_client_callback)
    : ModelProvider(segment_id), get_client_callback_(get_client_callback) {
  ON_CALL(*this, InitAndFetchModel(_))
      .WillByDefault(
          Invoke([&](const ModelUpdatedCallback& model_updated_callback) {
            get_client_callback_.Run(model_updated_callback);
          }));
}
MockModelProvider::~MockModelProvider() = default;

TestModelProviderFactory::Data::Data() = default;
TestModelProviderFactory::Data::~Data() = default;

std::unique_ptr<ModelProvider> TestModelProviderFactory::CreateProvider(
    proto::SegmentId segment_id) {
  auto provider = std::make_unique<MockModelProvider>(
      segment_id, base::BindRepeating(&StoreClientCallback, segment_id, data_));
  data_->model_providers.emplace(std::make_pair(segment_id, provider.get()));
  return provider;
}

std::unique_ptr<ModelProvider> TestModelProviderFactory::CreateDefaultProvider(
    proto::SegmentId segment_id) {
  if (!base::Contains(data_->segments_supporting_default_model, segment_id))
    return nullptr;

  auto provider = std::make_unique<MockModelProvider>(
      segment_id, base::BindRepeating(&StoreClientCallback, segment_id, data_));
  data_->default_model_providers.emplace(
      std::make_pair(segment_id, provider.get()));
  return provider;
}

}  // namespace segmentation_platform
