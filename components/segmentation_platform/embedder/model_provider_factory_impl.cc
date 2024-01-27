// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/model_provider_factory_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace {

class DummyModelProvider : public ModelProvider {
 public:
  DummyModelProvider()
      : ModelProvider(proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {}
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override {}

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }

  bool ModelAvailable() override { return false; }
};

}  // namespace

ModelProviderFactoryImpl::ModelProviderFactoryImpl(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_provider,
    std::vector<std::unique_ptr<Config>>& configs,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : optimization_guide_provider_(optimization_guide_provider),
      background_task_runner_(background_task_runner) {
  for (auto& config : configs) {
    for (auto& segment : config->segments) {
      if (segment.second->default_provider) {
        auto inserted = default_models_.insert(
            {segment.first, std::move(segment.second->default_provider)});
        DCHECK(inserted.second)
            << "Only one config can set default provider for " << segment.first;
      }
    }
  }
}

ModelProviderFactoryImpl::~ModelProviderFactoryImpl() = default;

std::unique_ptr<ModelProvider> ModelProviderFactoryImpl::CreateProvider(
    proto::SegmentId segment_id) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!optimization_guide_provider_) {
    // Optimization guide may not be available in some tests,
    return std::make_unique<DummyModelProvider>();
  }
  return std::make_unique<OptimizationGuideSegmentationModelProvider>(
      optimization_guide_provider_, background_task_runner_, segment_id);
#else
  return std::make_unique<DummyModelProvider>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

std::unique_ptr<DefaultModelProvider>
ModelProviderFactoryImpl::CreateDefaultProvider(proto::SegmentId segment_id) {
  auto test_override =
      TestDefaultModelOverride::GetInstance().TakeOwnershipOfModelProvider(
          segment_id);
  if (test_override) {
    return test_override;
  }

  auto it = default_models_.find(segment_id);
  if (it == default_models_.end()) {
    return nullptr;
  }
  DCHECK(it->second)
      << "Default model can be requested only once for a service.";
  return std::move(it->second);
}

TestDefaultModelOverride::TestDefaultModelOverride() = default;
TestDefaultModelOverride::~TestDefaultModelOverride() = default;

TestDefaultModelOverride& TestDefaultModelOverride::GetInstance() {
  static base::NoDestructor<TestDefaultModelOverride> instance;
  return *instance;
}

std::unique_ptr<DefaultModelProvider>
TestDefaultModelOverride::TakeOwnershipOfModelProvider(
    proto::SegmentId target) {
  auto it = default_providers_.find(target);
  if (it != default_providers_.end()) {
    DCHECK(it->second);
    return std::move(it->second);
  }
  return nullptr;
}

void TestDefaultModelOverride::SetModelForTesting(
    proto::SegmentId target,
    std::unique_ptr<DefaultModelProvider> default_provider) {
  default_providers_[target] = std::move(default_provider);
}

}  // namespace segmentation_platform
