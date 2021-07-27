// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

namespace {

Config CreateTestConfig() {
  Config config;
  config.segmentation_key = "some_key";
  config.segment_selection_ttl = base::TimeDelta::FromDays(28);
  config.segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
  return config;
}
}  // namespace

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

class SegmentSelectorTest : public testing::Test {
 public:
  SegmentSelectorTest() = default;
  ~SegmentSelectorTest() override = default;

  void SetUp() override {
    config_ = CreateTestConfig();
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    prefs_ = std::make_unique<MockSegmentationResultPrefs>();
    segment_selector_ = std::make_unique<SegmentSelectorImpl>(
        segment_database_.get(), prefs_.get(), &config_);
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
  Config config_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSegmentationResultPrefs> prefs_;
  std::unique_ptr<SegmentSelectorImpl> segment_selector_;
};

TEST_F(SegmentSelectorTest, CheckDiscreteMapping) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id, mapping, 3,
                                        config_.segmentation_key);
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(segment_id);
  const proto::SegmentationModelMetadata& metadata =
      segment_info->model_metadata();

  ASSERT_EQ(0, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.1,
                                      metadata));
  ASSERT_EQ(1, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.4,
                                      metadata));
  ASSERT_EQ(3, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.5,
                                      metadata));
  ASSERT_EQ(3, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.6,
                                      metadata));
  ASSERT_EQ(4, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.9,
                                      metadata));
}

TEST_F(SegmentSelectorTest, CheckMissingDiscreteMapping) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(segment_id);
  const proto::SegmentationModelMetadata& metadata =
      segment_info->model_metadata();

  // Any value should result in a 0 mapping, since no mapping exists.
  ASSERT_EQ(0, ConvertToDiscreteScore(segment_id, config_.segmentation_key, 0.9,
                                      metadata));
}

TEST_F(SegmentSelectorTest, CheckDefaultDiscreteMapping) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping_specific[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  float mapping_default[][2] = {{0.2, 5}, {0.5, 6}, {0.7, 7}};
  segment_database_->AddDiscreteMapping(segment_id, mapping_specific, 3,
                                        config_.segmentation_key);
  segment_database_->AddDiscreteMapping(segment_id, mapping_default, 3,
                                        "my-default");
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(segment_id);
  proto::SegmentationModelMetadata* metadata =
      segment_info->mutable_model_metadata();

  // No valid mapping should be found since there is no default mapping.
  EXPECT_EQ(0, ConvertToDiscreteScore(segment_id, "non-existing-key", 0.6,
                                      *metadata));

  metadata->set_default_discrete_mapping("my-default");
  // Should now use the default values instead of the one from the
  // one in the configuration key.
  EXPECT_EQ(6, ConvertToDiscreteScore(segment_id, "non-existing-key", 0.6,
                                      *metadata));
}

TEST_F(SegmentSelectorTest, CheckMissingDefaultDiscreteMapping) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping_default[][2] = {{0.2, 5}, {0.5, 6}, {0.7, 7}};
  segment_database_->AddDiscreteMapping(segment_id, mapping_default, 3,
                                        "my-default");
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(segment_id);
  proto::SegmentationModelMetadata* metadata =
      segment_info->mutable_model_metadata();
  metadata->set_default_discrete_mapping("not-my-default");

  // Should not find 'not-my-default' mapping, since it is registered as
  // 'my-default', so we should get a 0 result.
  EXPECT_EQ(0, ConvertToDiscreteScore(segment_id, "non-existing-key", 0.6,
                                      *metadata));
}

TEST_F(SegmentSelectorTest, FindBestSegmentFlowWithTwoSegments) {
  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  segment_database_->AddDiscreteMapping(segment_id, mapping, 3,
                                        config_.segmentation_key);

  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  segment_database_->AddDiscreteMapping(segment_id2, mapping2, 2,
                                        config_.segmentation_key);

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
                                        config_.segmentation_key);

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
                                        config_.segmentation_key);

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
      segment_database_.get(), prefs_.get(), &config_);

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
                                        config_.segmentation_key);

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
