// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/public/model_provider.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#else
#include "components/segmentation_platform/internal/execution/dummy_model_execution_manager.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace segmentation_platform {

std::unique_ptr<ModelExecutionManager> CreateModelExecutionManager(
    std::unique_ptr<ModelProviderFactory> model_provider_factory,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::flat_set<optimization_guide::proto::OptimizationTarget>&
        segment_ids,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    SignalDatabase* signal_database,
    FeatureListQueryProcessor* feature_list_query_processor,
    const ModelExecutionManager::SegmentationModelUpdatedCallback&
        model_updated_callback) {
  // TODO(ssid): The execution manager should always be created, since it can
  // run default models.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return std::make_unique<ModelExecutionManagerImpl>(
      segment_ids, std::move(model_provider_factory), clock, segment_database,
      signal_database, feature_list_query_processor, model_updated_callback);
#else
  return std::make_unique<DummyModelExecutionManager>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

}  // namespace segmentation_platform
