// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_

#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"

#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockFeatureListQueryProcessor : public FeatureListQueryProcessor {
 public:
  MockFeatureListQueryProcessor();
  ~MockFeatureListQueryProcessor() override;
  MOCK_METHOD(void,
              ProcessFeatureList,
              (const proto::SegmentationModelMetadata&,
               OptimizationTarget,
               base::Time,
               FeatureProcessorCallback),
              (override));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_
