// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/sql_feature_processor.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"

namespace segmentation_platform {

SqlFeatureProcessor::SqlFeatureProcessor(
    std::map<FeatureIndex, proto::SqlFeature> queries)
    : queries_(std::move(queries)) {}
SqlFeatureProcessor::~SqlFeatureProcessor() = default;

void SqlFeatureProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  // TODO(haileywang): Implement usage of custom input processor for bind
  // values.
  queries_.clear();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(feature_processor_state),
                     std::move(result_)));
}

}  // namespace segmentation_platform
