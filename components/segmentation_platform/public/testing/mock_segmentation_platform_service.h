// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_SEGMENTATION_PLATFORM_SERVICE_H_

#include <utility>

#include "base/strings/strcat.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class MockSegmentationPlatformService : public SegmentationPlatformService {
 public:
  MockSegmentationPlatformService() = default;
  ~MockSegmentationPlatformService() override = default;
  MOCK_METHOD(void,
              GetSelectedSegment,
              (const std::string&, SegmentSelectionCallback));
  MOCK_METHOD(SegmentSelectionResult,
              GetCachedSegmentResult,
              (const std::string&));
  MOCK_METHOD(void,
              GetClassificationResult,
              (const std::string&,
               const PredictionOptions&,
               scoped_refptr<InputContext>,
               ClassificationResultCallback));
  MOCK_METHOD(void,
              GetAnnotatedNumericResult,
              (const std::string&,
               const PredictionOptions&,
               scoped_refptr<InputContext>,
               AnnotatedNumericResultCallback));
  MOCK_METHOD(void,
              CollectTrainingData,
              (proto::SegmentId,
               TrainingRequestId,
               const TrainingLabels&,
               SuccessCallback));
  MOCK_METHOD(void,
              CollectTrainingData,
              (proto::SegmentId,
               TrainingRequestId,
               ukm::SourceId,
               const TrainingLabels&,
               SuccessCallback));
  MOCK_METHOD(void, EnableMetrics, (bool));
  MOCK_METHOD(void, GetServiceStatus, ());
  MOCK_METHOD(bool, IsPlatformInitialized, ());
  MOCK_METHOD(DatabaseClient*, GetDatabaseClient, ());
};

MATCHER_P(IsInputContextWithArgs,
          input_context_args,
          ::testing::PrintToString(input_context_args)) {
  return testing::ExplainMatchResult(
      testing::Field(
          &InputContext::metadata_args,
          testing::Eq(base::flat_map<std::string, processing::ProcessedValue>(
              input_context_args))),
      arg, result_listener);
}

MATCHER(TrainingLabelEmpty, "no training labels present") {
  return testing::ExplainMatchResult(
      testing::Field(&TrainingLabels::output_metric, testing::Eq(std::nullopt)),
      arg, result_listener);
}

MATCHER_P2(HasTrainingLabel,
           histogram_name,
           histogram_value,
           base::StrCat({"histogram with name", histogram_name, " and value ",
                         testing::PrintToString(histogram_value)})) {
  return testing::ExplainMatchResult(
      testing::Field(
          &TrainingLabels::output_metric,
          testing::Eq(std::pair<std::string, base::HistogramBase::Sample>(
              histogram_name, histogram_value))),
      arg, result_listener);
}

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_SEGMENTATION_PLATFORM_SERVICE_H_
