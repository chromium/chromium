// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_

#include <map>
#include <memory>
#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

// Mock model provider for testing, to be used with TestModelProviderFactory.
class MockModelProvider : public ModelProvider {
 public:
  MockModelProvider(
      optimization_guide::proto::OptimizationTarget segment_id,
      base::RepeatingCallback<void(const ModelProvider::ModelUpdatedCallback&)>
          get_client_callback);
  ~MockModelProvider() override;

  MOCK_METHOD(void,
              ExecuteModelWithInput,
              (const std::vector<float>& input,
               base::OnceCallback<void(const absl::optional<float>&)> callback),
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
    std::map<optimization_guide::proto::OptimizationTarget, MockModelProvider*>
        model_providers;
    // Map from target to updated callback, recorded when InitAndFetchModel()
    // was called on any provider.
    std::map<optimization_guide::proto::OptimizationTarget,
             ModelProvider::ModelUpdatedCallback>
        model_providers_callbacks;
  };

  // Records requests to `data`. `data` is not owned, and the caller must ensure
  // it is valid when the factory or provider is in use. Note that providers can
  // live longer than factory.
  explicit TestModelProviderFactory(Data* data) : data_(data) {}

  // ModelProviderFactory impl, that keeps track of the created provider and
  // callbacks in |data_|.
  std::unique_ptr<ModelProvider> CreateProvider(
      optimization_guide::proto::OptimizationTarget segment_id) override;

 private:
  raw_ptr<Data> data_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_MODEL_PROVIDER_H_
