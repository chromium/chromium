// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"

#include <cmath>
#include <optional>
#include <string_view>

#include "base/bit_cast.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using Segmentation_ModelExecution = ukm::builders::Segmentation_ModelExecution;

namespace segmentation_platform {

namespace {

// Round errors allowed during conversion.
static const double kRoundingError = 1E-5;

float Int64ToFloat(int64_t encoded) {
  return static_cast<float>(base::bit_cast<double>(encoded));
}

void CompareEncodeDecodeDifference(float tensor) {
  ASSERT_LT(
      std::abs(tensor -
               Int64ToFloat(
                   segmentation_platform::SegmentationUkmHelper::FloatToInt64(
                       tensor))),
      kRoundingError);
}

std::optional<proto::PredictionResult> GetPredictionResult(
    std::optional<base::Time> prediction_time = std::nullopt) {
  proto::PredictionResult result;
  result.add_result(0.5);
  result.add_result(0.4);
  if (prediction_time.has_value()) {
    result.set_timestamp_us(
        prediction_time->ToDeltaSinceWindowsEpoch().InMicroseconds());
  }
  return result;
}

proto::SegmentInfo CreateTestSegmentInfo(proto::SegmentId segment_id,
                                         bool upload_tensors) {
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(segment_id);
  segment_info.mutable_model_metadata()->set_upload_tensors(upload_tensors);
  return segment_info;
}

}  // namespace

class SegmentationUkmHelperTest : public testing::Test {
 public:
  SegmentationUkmHelperTest() = default;

  SegmentationUkmHelperTest(const SegmentationUkmHelperTest&) = delete;
  SegmentationUkmHelperTest& operator=(const SegmentationUkmHelperTest&) =
      delete;

  ~SegmentationUkmHelperTest() override = default;

  void SetUp() override {
    InitializeUkmHelper();
    test_recorder_.Purge();
  }

  void InitializeUkmHelper() {
    SegmentationUkmHelper::GetInstance()->Initialize();
  }

  void ExpectUkmMetrics(std::string_view entry_name,
                        const std::vector<std::string_view>& keys,
                        const std::vector<int64_t>& values,
                        ukm::SourceId source_id = ukm::kInvalidSourceId) {
    const auto& entries = test_recorder_.GetEntriesByName(entry_name);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* entry : entries) {
      if (source_id != ukm::kInvalidSourceId) {
        EXPECT_EQ(entry->source_id, source_id);
      }
      const size_t keys_size = keys.size();
      EXPECT_EQ(keys_size, values.size());
      for (size_t i = 0; i < keys_size; ++i) {
        test_recorder_.ExpectEntryMetric(entry, keys[i], values[i]);
      }
    }
  }

  void ExpectEmptyUkmMetrics(std::string_view entry_name) {
    EXPECT_EQ(0u, test_recorder_.GetEntriesByName(entry_name).size());
  }

  void SetSamplingRate(int sampling_rate) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSegmentationPlatformModelExecutionSampling,
        {{kModelExecutionSamplingRateKey,
          base::NumberToString(sampling_rate)}});
    InitializeUkmHelper();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that basic execution results recording works properly.
TEST_F(SegmentationUkmHelperTest, TestExecutionResultReporting) {
  SetSamplingRate(1);
  // Allow results for OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB to be recorded.
  ModelProvider::Request input_tensors = {0.1, 0.7, 0.8, 0.5};
  SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
      {0.6, 0.3});
  ExpectUkmMetrics(Segmentation_ModelExecution::kEntryName,
                   {Segmentation_ModelExecution::kOptimizationTargetName,
                    Segmentation_ModelExecution::kModelVersionName,
                    Segmentation_ModelExecution::kInput0Name,
                    Segmentation_ModelExecution::kInput1Name,
                    Segmentation_ModelExecution::kInput2Name,
                    Segmentation_ModelExecution::kInput3Name,
                    Segmentation_ModelExecution::kPredictionResult1Name,
                    Segmentation_ModelExecution::kPredictionResult2Name},
                   {
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       101,
                       SegmentationUkmHelper::FloatToInt64(0.1),
                       SegmentationUkmHelper::FloatToInt64(0.7),
                       SegmentationUkmHelper::FloatToInt64(0.8),
                       SegmentationUkmHelper::FloatToInt64(0.5),
                       SegmentationUkmHelper::FloatToInt64(0.6),
                       SegmentationUkmHelper::FloatToInt64(0.3),
                   });
}

// Tests that execution results recording are disabled if sampling rate is 0.
TEST_F(SegmentationUkmHelperTest,
       TestExecutionResultReportingwithZeroSampling) {
  SetSamplingRate(0);
  // Allow results for OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB to be recorded.
  ModelProvider::Request input_tensors = {0.1, 0.7, 0.8, 0.5};
  EXPECT_EQ(SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
                proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101,
                input_tensors, {0.6, 0.3}),
            ukm::NoURLSourceId());
}

// Tests that the training data collection recording works properly.
TEST_F(SegmentationUkmHelperTest, TestTrainingDataCollectionReporting) {
  ModelProvider::Request input_tensors = {0.1};
  ModelProvider::Response outputs = {1.0, 0.0};
  std::vector<int> output_indexes = {2, 3};

  SelectedSegment selected_segment(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 10);
  selected_segment.selection_time = base::Time::Now() - base::Seconds(10);
  SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101,
      /*ukm_source_id=*/55, input_tensors, outputs, output_indexes,
      GetPredictionResult(selected_segment.selection_time), selected_segment);
  ExpectUkmMetrics(Segmentation_ModelExecution::kEntryName,
                   {Segmentation_ModelExecution::kOptimizationTargetName,
                    Segmentation_ModelExecution::kModelVersionName,
                    Segmentation_ModelExecution::kInput0Name,
                    Segmentation_ModelExecution::kActualResult3Name,
                    Segmentation_ModelExecution::kActualResult4Name,
                    Segmentation_ModelExecution::kPredictionResult1Name,
                    Segmentation_ModelExecution::kPredictionResult2Name,
                    Segmentation_ModelExecution::kSelectionResultName,
                    Segmentation_ModelExecution::kOutputDelaySecName},
                   {
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       101,
                       SegmentationUkmHelper::FloatToInt64(0.1),
                       SegmentationUkmHelper::FloatToInt64(1.0),
                       SegmentationUkmHelper::FloatToInt64(0.0),
                       SegmentationUkmHelper::FloatToInt64(0.5),
                       SegmentationUkmHelper::FloatToInt64(0.4),
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       10,
                   },
                   /*source_id=*/55);
}

// Tests tensor uploading for default allowed list.
TEST_F(SegmentationUkmHelperTest, TestDefaultAllowedList) {
  proto::SegmentInfo segment_info =
      CreateTestSegmentInfo(proto::OPTIMIZATION_TARGET_UNKNOWN, false);
  EXPECT_FALSE(
      SegmentationUkmHelper::GetInstance()->IsUploadRequested(segment_info));
  segment_info = CreateTestSegmentInfo(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, false);
  EXPECT_TRUE(
      SegmentationUkmHelper::GetInstance()->IsUploadRequested(segment_info));
}

// Tests that tensor uploading if default allowed list is disabled.
TEST_F(SegmentationUkmHelperTest, TestDisallowDefaultAllowedList) {
  feature_list_.InitAndDisableFeature(
      features::kSegmentationDefaultReportingSegments);
  InitializeUkmHelper();
  proto::SegmentInfo segment_info = CreateTestSegmentInfo(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, false);
  EXPECT_FALSE(
      SegmentationUkmHelper::GetInstance()->IsUploadRequested(segment_info));
}

// Tests that tensor uploading is enabled through metadata.
TEST_F(SegmentationUkmHelperTest, TestUploadTensorsAllowedFromMetadata) {
  proto::SegmentInfo segment_info = CreateTestSegmentInfo(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, true);
  EXPECT_TRUE(
      SegmentationUkmHelper::GetInstance()->IsUploadRequested(segment_info));
}

// Tests that float encoding works properly.
TEST_F(SegmentationUkmHelperTest, TestFloatEncoding) {
  // Compare the numbers with their IEEE754 binary representations in double.
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.5), 0x3FE0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.25), 0x3FD0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.125), 0x3FC0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.75), 0x3FE8000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(1), 0x3FF0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0), 0);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(10), 0x4024000000000000);
}

// Tests that floats encoded can be properly decoded later.
TEST_F(SegmentationUkmHelperTest, FloatEncodeDeocode) {
  CompareEncodeDecodeDifference(0.1);
  CompareEncodeDecodeDifference(0.5);
  CompareEncodeDecodeDifference(0.88);
  CompareEncodeDecodeDifference(0.01);
  ASSERT_EQ(0, Int64ToFloat(SegmentationUkmHelper::FloatToInt64(0)));
  ASSERT_EQ(1, Int64ToFloat(SegmentationUkmHelper::FloatToInt64(1)));
}

// Tests that there are too many input tensors to record.
TEST_F(SegmentationUkmHelperTest, TooManyInputTensors) {
  SetSamplingRate(1);
  base::HistogramTester tester;
  std::string histogram_name(
      "SegmentationPlatform.StructuredMetrics.TooManyTensors.Count");
  ModelProvider::Request input_tensors(100, 0.1);
  ukm::SourceId source_id =
      SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
          proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
          {0.6});
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);
  tester.ExpectTotalCount(histogram_name, 1);
  ASSERT_EQ(tester.GetTotalSum(histogram_name), 100);
}

// Tests output validation for |RecordTrainingData|.
TEST_F(SegmentationUkmHelperTest, OutputsValidation) {
  ModelProvider::Request input_tensors{0.1};

  // outputs, output_indexes size doesn't match.
  ModelProvider::Response outputs{1.0, 0.0};
  std::vector<int> output_indexes{0};

  ukm::SourceId source_id =
      SegmentationUkmHelper::GetInstance()->RecordTrainingData(
          proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101,
          ukm::kInvalidSourceId, input_tensors, outputs, output_indexes,
          GetPredictionResult(), std::nullopt);
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);

  // output_indexes value too large.
  output_indexes = {100, 1000};
  source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101,
      ukm::kInvalidSourceId, input_tensors, outputs, output_indexes,
      GetPredictionResult(), std::nullopt);
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);

  // Valid outputs.
  output_indexes = {3, 0};
  source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101,
      ukm::kInvalidSourceId, input_tensors, outputs, output_indexes,
      GetPredictionResult(), std::nullopt);
  ASSERT_NE(source_id, ukm::kInvalidSourceId);
}

TEST_F(SegmentationUkmHelperTest, AllowedToUploadData) {
  TestingPrefServiceSimple prefs;
  SegmentationPlatformService::RegisterLocalStatePrefs(prefs.registry());
  LocalStateHelper::GetInstance().Initialize(&prefs);
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());

  // If pref is not initialized, AllowedToUploadData() always return false.
  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));

  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey, clock.Now());

  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));
  clock.Advance(base::Seconds(10));
  ASSERT_TRUE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));
  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(11), &clock));
}

}  // namespace segmentation_platform
