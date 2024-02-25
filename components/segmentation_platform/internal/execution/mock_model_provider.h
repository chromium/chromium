// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

using proto::SegmentId;

// Mock model provider for testing, to be used with TestModelProviderFactory.
class MockModelProvider : public ModelProvider {
 public:
  MockModelProvider(
      proto::SegmentId segment_id,
      base::RepeatingCallback<void(const ModelProvider::ModelUpdatedCallback&)>
          get_client_callback);
  ~MockModelProvider() override;

  MOCK_METHOD(
      void,
      ExecuteModelWithInput,
      (const ModelProvider::Request& input,
       base::OnceCallback<void(const std::optional<ModelProvider::Response>&)>
           callback),
      (override));

  MOCK_METHOD(void,
              InitAndFetchModel,
              (const ModelUpdatedCallback& model_updated_callback),
              (override));

  MOCK_METHOD(bool, ModelAvailable, (), (override));

 private:
  base::RepeatingCallback<void(const ModelProvider::ModelUpdatedCallback&)>
      get_client_callback_;
};

class MockDefaultModelProvider : public DefaultModelProvider {
 public:
  MockDefaultModelProvider(proto::SegmentId segment_id,
                           const proto::SegmentationModelMetadata& metadata);
  ~MockDefaultModelProvider() override;

  MOCK_METHOD(
      void,
      ExecuteModelWithInput,
      (const ModelProvider::Request& input,
       base::OnceCallback<void(const std::optional<ModelProvider::Response>&)>
           callback),
      (override));

  MOCK_METHOD(std::unique_ptr<DefaultModelProvider::ModelConfig>,
              GetModelConfig,
              (),
              (override));

 private:
  proto::SegmentationModelMetadata metadata_;
};

// Test factory for providers, keeps track of model requests, but does not run
// the model callbacks.
class TestModelProviderFactory : public ModelProviderFactory {
 public:
  // List of model requests given to the providers.
  struct Data {
    Data();
    ~Data();

    // Map of targets to model providers, added when provider is created. The
    // list is not cleared when providers are destroyed.
    std::map<proto::SegmentId, MockModelProvider*> model_providers;

    // Map of targets to default model providers, added when provider is
    // created. The list is not cleared when providers are destroyed.
    std::map<proto::SegmentId, MockDefaultModelProvider*>
        default_model_providers;

    // Map of targets to the metadata that the default model should return when
    // the platform requests for the data.
    std::map<proto::SegmentId, proto::SegmentationModelMetadata>
        default_provider_metadata;

    // Map from target to updated callback, recorded when InitAndFetchModel()
    // was called on any provider.
    std::map<proto::SegmentId, ModelProvider::ModelUpdatedCallback>
        model_providers_callbacks;

    base::flat_set<SegmentId> segments_supporting_default_model;
  };

  // Records requests to `data`. `data` is not owned, and the caller must ensure
  // it is valid when the factory or provider is in use. Note that providers can
  // live longer than factory.
  explicit TestModelProviderFactory(Data* data) : data_(data) {}

  // ModelProviderFactory impl, that keeps track of the created provider and
  // callbacks in |data_|.
  std::unique_ptr<ModelProvider> CreateProvider(
      proto::SegmentId segment_id) override;

  std::unique_ptr<DefaultModelProvider> CreateDefaultProvider(
      proto::SegmentId) override;

 private:
  raw_ptr<Data> data_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_
