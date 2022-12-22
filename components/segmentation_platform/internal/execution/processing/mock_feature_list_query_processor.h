// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_

#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"

#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform::processing {

class MockFeatureListQueryProcessor : public FeatureListQueryProcessor {
 public:
  MockFeatureListQueryProcessor();
  ~MockFeatureListQueryProcessor() override;
  MOCK_METHOD(void,
              ProcessFeatureList,
              (const proto::SegmentationModelMetadata&,
               scoped_refptr<InputContext> input_context,
               proto::SegmentId,
               base::Time,
               base::Time,
               FeatureListQueryProcessor::ProcessOption,
               FeatureProcessorCallback),
              (override));
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_LIST_QUERY_PROCESSOR_H_
