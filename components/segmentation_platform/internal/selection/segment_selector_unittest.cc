// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {

class MockSegmentationResultPrefs : public SegmentationResultPrefs {
 public:
  MockSegmentationResultPrefs() : SegmentationResultPrefs(nullptr) {}
  MOCK_METHOD(void,
              SaveSegmentationResultToPref,
              (const std::string&, const absl::optional<SelectedSegment>&));
  MOCK_METHOD(absl::optional<SelectedSegment>,
              ReadSegmentationResultFromPref,
              (const std::string&));
};

class MockModelExecutionScheduler : public ModelExecutionScheduler {
 public:
  MockModelExecutionScheduler() = default;
  ~MockModelExecutionScheduler() override = default;
  MOCK_METHOD(void, OnNewModelInfoReady, (OptimizationTarget));
  MOCK_METHOD(void, RequestModelExecutionForEligibleSegments, (bool));
  MOCK_METHOD(void, RequestModelExecution, (OptimizationTarget));
  MOCK_METHOD(void,
              OnModelExecutionCompleted,
              (OptimizationTarget,
               (const std::pair<float, ModelExecutionStatus>&)));
};

class SegmentSelectorTest : public testing::Test {
 public:
  SegmentSelectorTest() = default;
  ~SegmentSelectorTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    prefs_ = std::make_unique<MockSegmentationResultPrefs>();
    segment_selector_ = std::make_unique<SegmentSelectorImpl>(
        segment_database_.get(), prefs_.get(), kAdaptiveToolbarSegmentationKey);
    segment_selector_->set_model_execution_scheduler(
        &model_execution_scheduler_);
  }

  int ConvertToDiscreteScore(OptimizationTarget segment_id,
                             const std::string& mapping_key,
                             float score,
                             const proto::SegmentationModelMetadata& metadata) {
    return segment_selector_->ConvertToDiscreteScore(segment_id, mapping_key,
                                                     score, metadata);
  }

  void GetSelectedSegment(const SegmentSelectionResult& expected) {
    base::RunLoop loop;
    segment_selector_->GetSelectedSegment(
        base::BindOnce(&SegmentSelectorTest::OnGetSelectedSegment,
                       base::Unretained(this), loop.QuitClosure(), expected));
    loop.Run();
  }

  void OnGetSelectedSegment(base::RepeatingClosure closure,
                            const SegmentSelectionResult& expected,
                            const SegmentSelectionResult& actual) {
    ASSERT_EQ(expected, actual);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSegmentationResultPrefs> prefs_;
  std::unique_ptr<SegmentSelectorImpl> segment_selector_;
  MockModelExecutionScheduler model_execution_scheduler_;
};

TEST_F(SegmentSelectorTest, CheckDiscreteMapping) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  std::string segmentation_key = kAdaptiveToolbarSegmentationKey;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id, mapping, 3,
                                        segmentation_key);
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(segment_id);
  const proto::SegmentationModelMetadata& metadata =
      segment_info->model_metadata();

  ASSERT_EQ(
      0, ConvertToDiscreteScore(segment_id, segmentation_key, 0.1, metadata));
  ASSERT_EQ(
      1, ConvertToDiscreteScore(segment_id, segmentation_key, 0.4, metadata));
  ASSERT_EQ(
      3, ConvertToDiscreteScore(segment_id, segmentation_key, 0.5, metadata));
  ASSERT_EQ(
      3, ConvertToDiscreteScore(segment_id, segmentation_key, 0.6, metadata));
  ASSERT_EQ(
      4, ConvertToDiscreteScore(segment_id, segmentation_key, 0.9, metadata));
}

TEST_F(SegmentSelectorTest, FindBestSegmentFlowWithTwoSegments) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id, mapping, 3,
                                        kAdaptiveToolbarSegmentationKey);

  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  segment_database_->AddDiscreteMapping(segment_id2, mapping2, 2,
                                        kAdaptiveToolbarSegmentationKey);

  base::Time result_timestamp = base::Time::Now();
  segment_database_->AddPredictionResult(segment_id, 0.6, result_timestamp);
  segment_database_->AddPredictionResult(segment_id2, 0.5, result_timestamp);

  absl::optional<SelectedSegment> selected_segment;
  EXPECT_CALL(*prefs_, SaveSegmentationResultToPref(_, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&selected_segment));

  segment_selector_->OnModelExecutionCompleted(segment_id);
  ASSERT_TRUE(selected_segment.has_value());
  ASSERT_EQ(segment_id2, selected_segment->segment_id);
}

TEST_F(SegmentSelectorTest, NewSegmentResultOverridesThePreviousBest) {
  OptimizationTarget segment_id1 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id1, mapping1, 3,
                                        kAdaptiveToolbarSegmentationKey);

  base::Time result_timestamp = base::Time::Now();
  segment_database_->AddPredictionResult(segment_id1, 0.6, result_timestamp);

  absl::optional<SelectedSegment> selected_segment;
  EXPECT_CALL(*prefs_, SaveSegmentationResultToPref(_, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&selected_segment));

  segment_selector_->OnModelExecutionCompleted(segment_id1);
  ASSERT_TRUE(selected_segment.has_value());
  ASSERT_EQ(segment_id1, selected_segment->segment_id);

  // Another model completes execution. The selection should update.
  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  segment_database_->AddDiscreteMapping(segment_id2, mapping2, 2,
                                        kAdaptiveToolbarSegmentationKey);

  segment_database_->AddPredictionResult(segment_id2, 0.5, result_timestamp);
  EXPECT_CALL(*prefs_, SaveSegmentationResultToPref(_, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&selected_segment));

  segment_selector_->OnModelExecutionCompleted(segment_id2);
  ASSERT_TRUE(selected_segment.has_value());
  ASSERT_EQ(segment_id2, selected_segment->segment_id);
}

TEST_F(SegmentSelectorTest,
       GetSelectedSegmentReturnsResultFromPreviousSession) {
  // Set up a selected segment in prefs.
  OptimizationTarget segment_id0 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  SelectedSegment from_history(segment_id0);
  EXPECT_CALL(*prefs_, ReadSegmentationResultFromPref(_))
      .WillRepeatedly(Return(from_history));

  // Construct a segment selector. It should read result from last session.
  segment_selector_ = std::make_unique<SegmentSelectorImpl>(
      segment_database_.get(), prefs_.get(), kAdaptiveToolbarSegmentationKey);
  segment_selector_->set_model_execution_scheduler(&model_execution_scheduler_);

  base::RunLoop loop;
  segment_selector_->Initialize(loop.QuitClosure());
  loop.Run();

  SegmentSelectionResult result;
  result.segment = segment_id0;
  result.is_ready = true;
  GetSelectedSegment(result);

  // Add results for a new segment.
  OptimizationTarget segment_id1 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id1, mapping1, 3,
                                        kAdaptiveToolbarSegmentationKey);

  base::Time result_timestamp = base::Time::Now();
  segment_database_->AddPredictionResult(segment_id1, 0.6, result_timestamp);

  absl::optional<SelectedSegment> selected_segment;
  EXPECT_CALL(*prefs_, SaveSegmentationResultToPref(_, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&selected_segment));

  segment_selector_->OnModelExecutionCompleted(segment_id1);
  ASSERT_TRUE(selected_segment.has_value());
  ASSERT_EQ(segment_id1, selected_segment->segment_id);

  // GetSelectedSegment should still return value from previous session.
  GetSelectedSegment(result);
}

}  // namespace segmentation_platform
