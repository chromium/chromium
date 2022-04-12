// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
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
  config.segmentation_key = "test_key";
  config.segment_selection_ttl = base::Days(28);
  config.unknown_selection_ttl = base::Days(14);
  config.segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
  return config;
}

}  // namespace

class TestSegmentationResultPrefs : public SegmentationResultPrefs {
 public:
  TestSegmentationResultPrefs() : SegmentationResultPrefs(nullptr) {}

  void SaveSegmentationResultToPref(
      const std::string& result_key,
      const absl::optional<SelectedSegment>& selected_segment) override {
    selection = selected_segment;
  }

  absl::optional<SelectedSegment> ReadSegmentationResultFromPref(
      const std::string& result_key) override {
    return selection;
  }

  absl::optional<SelectedSegment> selection;
};

class SegmentSelectorTest : public testing::Test {
 public:
  SegmentSelectorTest() : provider_factory_(&model_providers_) {}
  ~SegmentSelectorTest() override = default;

  void SetUpWithConfig(const Config& config) {
    clock_.SetNow(base::Time::Now());
    config_ = config;
    default_manager_ = std::make_unique<DefaultModelManager>(
        &provider_factory_, config_.segment_ids);
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    auto prefs_moved = std::make_unique<TestSegmentationResultPrefs>();
    prefs_ = prefs_moved.get();
    segment_selector_ = std::make_unique<SegmentSelectorImpl>(
        segment_database_.get(), &signal_storage_config_,
        std::move(prefs_moved), &config_, &clock_,
        PlatformOptions::CreateDefault(), default_manager_.get());
    segment_selector_->OnPlatformInitialized(nullptr);
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

  void InitializeMetadataForSegment(OptimizationTarget segment_id,
                                    float mapping[][2],
                                    int num_mapping_pairs) {
    EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
        .WillRepeatedly(Return(true));
    segment_database_->FindOrCreateSegment(segment_id)
        ->mutable_model_metadata()
        ->set_result_time_to_live(7);
    segment_database_->SetBucketDuration(segment_id, 1, proto::TimeUnit::DAY);

    segment_database_->AddDiscreteMapping(
        segment_id, mapping, num_mapping_pairs, config_.segmentation_key);
  }

  void CompleteModelExecution(OptimizationTarget segment_id, float score) {
    segment_database_->AddPredictionResult(segment_id, score, clock_.Now());
    segment_selector_->OnModelExecutionCompleted(segment_id);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  TestModelProviderFactory::Data model_providers_;
  TestModelProviderFactory provider_factory_;
  Config config_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  MockSignalStorageConfig signal_storage_config_;
  std::unique_ptr<DefaultModelManager> default_manager_;
  raw_ptr<TestSegmentationResultPrefs> prefs_;
  std::unique_ptr<SegmentSelectorImpl> segment_selector_;
};

TEST_F(SegmentSelectorTest, FindBestSegmentFlowWithTwoSegments) {
  SetUpWithConfig(CreateTestConfig());
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));

  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(segment_id, mapping, 3);

  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  InitializeMetadataForSegment(segment_id2, mapping2, 2);

  segment_database_->AddPredictionResult(segment_id, 0.6, clock_.Now());
  segment_database_->AddPredictionResult(segment_id2, 0.5, clock_.Now());

  clock_.Advance(base::Days(1));
  segment_selector_->OnModelExecutionCompleted(segment_id);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id2, prefs_->selection->segment_id);
}

TEST_F(SegmentSelectorTest, NewSegmentResultOverridesThePreviousBest) {
  Config config = CreateTestConfig();
  config.unknown_selection_ttl = base::TimeDelta();
  SetUpWithConfig(config);

  // Setup test with two models.
  OptimizationTarget segment_id1 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}, {0.8, 5}};
  InitializeMetadataForSegment(segment_id1, mapping1, 4);

  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  InitializeMetadataForSegment(segment_id2, mapping2, 2);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));

  // Model 1 completes with a zero-ish score. We will wait for the other model
  // to finish before updating results.
  CompleteModelExecution(segment_id1, 0.1);
  ASSERT_FALSE(prefs_->selection.has_value());

  CompleteModelExecution(segment_id2, 0.1);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Model 1 completes with a good score. Model 2 results are expired.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id1, 0.6);
  ASSERT_EQ(OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Model 2 gets fresh results. Now segment selection will update.
  CompleteModelExecution(segment_id2, 0.1);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id1, prefs_->selection->segment_id);

  // Model 2 runs with a better score. The selection should update to model 2
  // after both models are run.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id1, 0.6);
  CompleteModelExecution(segment_id2, 0.5);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id2, prefs_->selection->segment_id);

  // Run the models again after few days later, but segment selection TTL hasn't
  // expired. Result will not update.
  clock_.Advance(config_.segment_selection_ttl * 0.8f);
  CompleteModelExecution(segment_id1, 0.8);
  CompleteModelExecution(segment_id2, 0.5);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id2, prefs_->selection->segment_id);

  // Rerun both models which report zero-ish scores. The previous selection
  // should be retained.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id1, 0.1);
  CompleteModelExecution(segment_id2, 0.1);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id2, prefs_->selection->segment_id);
}

TEST_F(SegmentSelectorTest, UnknownSegmentTtlExpiryForBooleanModel) {
  Config config = CreateTestConfig();
  config.segment_ids = {
      OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID};
  SetUpWithConfig(config);

  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID;
  float mapping[][2] = {{0.7, 1}};
  InitializeMetadataForSegment(segment_id, mapping, 1);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));

  // Set a value less than 1 and result should be UNKNOWN.
  CompleteModelExecution(segment_id, 0);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Advance by less than UNKNOWN segment TTL and result should not change,
  // UNKNOWN segment TTL is less than selection TTL.
  clock_.Advance(config_.unknown_selection_ttl * 0.8f);
  CompleteModelExecution(segment_id, 0.9);
  ASSERT_EQ(OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Advance clock so that the time is between UNKNOWN segment TTL and selection
  // TTL.
  clock_.Advance(config_.unknown_selection_ttl * 0.4f);
  CompleteModelExecution(segment_id, 0.9);
  ASSERT_EQ(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      prefs_->selection->segment_id);

  // Advance by more than UNKNOWN segment TTL and result should not change.
  clock_.Advance(config_.unknown_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id, 0);
  ASSERT_EQ(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      prefs_->selection->segment_id);

  // Advance by segment selection TTL and result should change.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id, 0);
  ASSERT_EQ(OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);
}

TEST_F(SegmentSelectorTest, DoesNotMeetSignalCollectionRequirement) {
  SetUpWithConfig(CreateTestConfig());
  OptimizationTarget segment_id1 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}, {0.8, 5}};

  segment_database_->FindOrCreateSegment(segment_id1)
      ->mutable_model_metadata()
      ->set_result_time_to_live(7);
  segment_database_->SetBucketDuration(segment_id1, 1, proto::TimeUnit::DAY);
  segment_database_->AddDiscreteMapping(segment_id1, mapping1, 4,
                                        config_.segmentation_key);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(false));

  CompleteModelExecution(segment_id1, 0.5);
  ASSERT_FALSE(prefs_->selection.has_value());
}

TEST_F(SegmentSelectorTest,
       GetSelectedSegmentReturnsResultFromPreviousSession) {
  SetUpWithConfig(CreateTestConfig());
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));
  OptimizationTarget segment_id0 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  OptimizationTarget segment_id1 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping0[][2] = {{1.0, 0}};
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(segment_id0, mapping0, 1);
  InitializeMetadataForSegment(segment_id1, mapping1, 3);

  // Set up a selected segment in prefs.
  SelectedSegment from_history(segment_id0);
  auto prefs_moved = std::make_unique<TestSegmentationResultPrefs>();
  prefs_ = prefs_moved.get();
  prefs_->selection = from_history;

  // Construct a segment selector. It should read result from last session.
  segment_selector_ = std::make_unique<SegmentSelectorImpl>(
      segment_database_.get(), &signal_storage_config_, std::move(prefs_moved),
      &config_, &clock_, PlatformOptions::CreateDefault(),
      default_manager_.get());
  segment_selector_->OnPlatformInitialized(nullptr);

  SegmentSelectionResult result;
  result.segment = segment_id0;
  result.is_ready = true;
  GetSelectedSegment(result);
  ASSERT_EQ(result, segment_selector_->GetCachedSegmentResult());

  // Add results for a new segment.
  base::Time result_timestamp = base::Time::Now();
  segment_database_->AddPredictionResult(segment_id1, 0.6, result_timestamp);
  segment_database_->AddPredictionResult(segment_id0, 0.5, result_timestamp);

  segment_selector_->OnModelExecutionCompleted(segment_id1);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id1, prefs_->selection->segment_id);

  // GetSelectedSegment should still return value from previous session.
  GetSelectedSegment(result);
  ASSERT_EQ(result, segment_selector_->GetCachedSegmentResult());
}

// Tests that prefs are properly updated after calling UpdateSelectedSegment().
TEST_F(SegmentSelectorTest, UpdateSelectedSegment) {
  SetUpWithConfig(CreateTestConfig());
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));

  OptimizationTarget segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(segment_id, mapping, 3);

  OptimizationTarget segment_id2 =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  InitializeMetadataForSegment(segment_id2, mapping2, 2);

  segment_database_->AddPredictionResult(segment_id, 0.6, clock_.Now());
  segment_database_->AddPredictionResult(segment_id2, 0.5, clock_.Now());

  clock_.Advance(base::Days(1));
  segment_selector_->OnModelExecutionCompleted(segment_id);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id2, prefs_->selection->segment_id);

  // Update the selected segment to |segment_id|.
  segment_selector_->UpdateSelectedSegment(segment_id);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(segment_id, prefs_->selection->segment_id);
}

}  // namespace segmentation_platform
