// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_MODEL_PROVIDER_FACTORY_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_MODEL_PROVIDER_FACTORY_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace segmentation_platform {

struct Config;

class ModelProviderFactoryImpl : public ModelProviderFactory {
 public:
  ModelProviderFactoryImpl(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_provider,
      std::vector<std::unique_ptr<Config>>& configs,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  ~ModelProviderFactoryImpl() override;

  ModelProviderFactoryImpl(const ModelProviderFactoryImpl&) = delete;
  ModelProviderFactoryImpl& operator=(const ModelProviderFactoryImpl&) = delete;

  // ModelProviderFactory impl:
  std::unique_ptr<ModelProvider> CreateProvider(
      proto::SegmentId segment_id) override;
  std::unique_ptr<DefaultModelProvider> CreateDefaultProvider(
      proto::SegmentId segment_id) override;

 private:
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_provider_;
  base::flat_map<proto::SegmentId, std::unique_ptr<DefaultModelProvider>>
      default_models_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

// Used only in tests to override the default model.
class TestDefaultModelOverride {
 public:
  static TestDefaultModelOverride& GetInstance();

  ~TestDefaultModelOverride();
  TestDefaultModelOverride(const TestDefaultModelOverride& client) = delete;
  TestDefaultModelOverride& operator=(const TestDefaultModelOverride& client) =
      delete;

  std::unique_ptr<DefaultModelProvider> TakeOwnershipOfModelProvider(
      proto::SegmentId target);

  void SetModelForTesting(proto::SegmentId target,
                          std::unique_ptr<DefaultModelProvider>);

 private:
  friend class base::NoDestructor<TestDefaultModelOverride>;

  TestDefaultModelOverride();

  std::map<proto::SegmentId, std::unique_ptr<DefaultModelProvider>>
      default_providers_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_MODEL_PROVIDER_FACTORY_IMPL_H_
