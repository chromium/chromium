// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "components/segmentation_platform/internal/execution/query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class FeatureProcessorState;

// SqlFeatureProcessor takes a list of SqlFeature type of input, fetches samples
// from the UKMDatabase, and computes an input tensor to use when executing the
// ML model.
class SqlFeatureProcessor : public QueryProcessor {
 public:
  explicit SqlFeatureProcessor(
      std::map<FeatureIndex, proto::SqlFeature> queries);
  ~SqlFeatureProcessor() override;

  // QueryProcessor implementation.
  void Process(std::unique_ptr<FeatureProcessorState> feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  // List of sql features to process into input tensors.
  std::map<FeatureIndex, proto::SqlFeature> queries_;

  // List of resulting input tensors.
  IndexedTensors result_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_
