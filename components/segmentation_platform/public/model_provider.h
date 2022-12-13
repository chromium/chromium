// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_

#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  using ModelUpdatedCallback = base::RepeatingCallback<
      void(proto::SegmentId, proto::SegmentationModelMetadata, int64_t)>;
  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<Response>&)>;

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
  // otherwise returns absl::nullopt. Implementation could be a heuristic or
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

// Interface used by segmentation platform to create ModelProvider(s).
class ModelProviderFactory {
 public:
  virtual ~ModelProviderFactory();

  // Creates a model provider for the given `segment_id`.
  virtual std::unique_ptr<ModelProvider> CreateProvider(proto::SegmentId) = 0;

  // Creates a default model provider to be used when the original provider did
  // not provide a model. Returns `nullptr` when a default provider is not
  // available.
  // TODO(crbug.com/1346389): This method should be moved to Config after
  // migrating all the tests that use this.
  virtual std::unique_ptr<ModelProvider> CreateDefaultProvider(
      proto::SegmentId) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_MODEL_PROVIDER_H_
