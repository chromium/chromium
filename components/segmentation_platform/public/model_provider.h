// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace proto {
class SegmentationModelMetadata;
}  // namespace proto

// Interface used by the segmentation platform to get model and metadata for a
// single optimization target.
class ModelProvider {
 public:
  using Request = std::vector<float>;
  using Response = std::vector<float>;

  using ModelUpdatedCallback = base::RepeatingCallback<void(
      proto::SegmentId,
      std::optional<proto::SegmentationModelMetadata>,
      int64_t)>;
  using ExecutionCallback =
      base::OnceCallback<void(const std::optional<Response>&)>;

  explicit ModelProvider(proto::SegmentId segment_id);
  virtual ~ModelProvider();

  ModelProvider(const ModelProvider&) = delete;
  ModelProvider& operator=(const ModelProvider&) = delete;

  // Implementation should return metadata that will be used to execute model.
  // The metadata provided should define the number of features needed by the
  // ExecuteModelWithInput() method. Starts a fetch request for the model for
  // optimization target. The `model_updated_callback` can be called multiple
  // times when new models are available for the optimization target.
  virtual void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) = 0;

  // Executes the latest model available, with the given inputs and returns
  // result via `callback`. Should be called only after InitAndFetchModel()
  // otherwise returns std::nullopt. Implementation could be a heuristic or
  // model execution to return a result. The inputs to this method are the
  // computed tensors based on the features provided in the latest call to
  // `model_updated_callback`. The result is a float score with the probability
  // of positive result. Also see `discrete_mapping` field in the
  // `SegmentationModelMetadata` for how the score will be used to determine the
  // segment.
  virtual void ExecuteModelWithInput(const Request& inputs,
                                     ExecutionCallback callback) = 0;

  // Returns true if a model is available.
  virtual bool ModelAvailable() = 0;

 protected:
  const proto::SegmentId segment_id_;
};

// ModelProvider wrapper for implementing default models in c++.
class DefaultModelProvider : public ModelProvider {
 public:
  explicit DefaultModelProvider(proto::SegmentId segment_id);
  ~DefaultModelProvider() override;

  DefaultModelProvider(const DefaultModelProvider&) = delete;
  DefaultModelProvider& operator=(const DefaultModelProvider&) = delete;

  // Config needed for the model.
  struct ModelConfig {
    // Model metadata that contains inputs, outputs, and other configuration
    // fields.
    proto::SegmentationModelMetadata metadata;
    // Model version. Should be incremented for any changes to the model.
    int64_t model_version;

    ModelConfig(proto::SegmentationModelMetadata metadata,
                int64_t model_version);
    ~ModelConfig();

    ModelConfig(const ModelConfig&) = delete;
    ModelConfig& operator=(const ModelConfig&) = delete;
  };
  virtual std::unique_ptr<ModelConfig> GetModelConfig() = 0;

  // Returns true by default. Can be overridden to disable the model if needed.
  bool ModelAvailable() override;

 private:
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) final;
};

// Interface used by segmentation platform to create ModelProvider(s).
class ModelProviderFactory {
 public:
  virtual ~ModelProviderFactory();

  // Creates a model provider for the given `segment_id`.
  virtual std::unique_ptr<ModelProvider> CreateProvider(proto::SegmentId) = 0;

  // Creates a default model provider to be used when the original provider did
  // not provide a model. Returns `nullptr` when a default provider is not
  // available.
  // TODO(crbug.com/40232484): This method should be moved to Config after
  // migrating all the tests that use this.
  virtual std::unique_ptr<DefaultModelProvider> CreateDefaultProvider(
      proto::SegmentId) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_
