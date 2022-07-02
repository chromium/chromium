// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metric_filter_utils.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

namespace {

class MockFieldTrialRegister : public FieldTrialRegister {
 public:
  MOCK_METHOD2(RegisterFieldTrial,
               void(base::StringPiece trial_name,
                    base::StringPiece group_name));

  MOCK_METHOD3(RegisterSubsegmentFieldTrialIfNeeded,
               void(base::StringPiece trial_name,
                    proto::SegmentId segment_id,
                    int subsegment_rank));
};

Config CreateTestConfig() {
  Config config;
  config.segmentation_key = "test_key";
  config.segment_selection_ttl = base::Days(28);
  config.unknown_selection_ttl = base::Days(14);
  config.segment_ids = {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
  return config;
}

class MockResultProvider : public SegmentResultProvider {
 public:
  MOCK_METHOD1(GetSegmentResult,
               void(std::unique_ptr<GetResultOptions> options));
};

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
        std::move(prefs_moved), &config_, &field_trial_register_, &clock_,
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

  void InitializeMetadataForSegment(SegmentId segment_id,
                                    float mapping[][2],
                                    int num_mapping_pairs) {
    EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
        .WillRepeatedly(Return(true));
    segment_database_->FindOrCreateSegment(segment_id)
        ->mutable_model_metadata()
        ->set_result_time_to_live(7);
    segment_database_->SetBucketDuration(segment_id, 1, proto::TimeUnit::DAY);

    segment_database_->AddDiscreteMapping(
        segment_id, mapping, num_mapping_pairs, config_.segmentation_key);
  }

  void CompleteModelExecution(SegmentId segment_id, float score) {
    segment_database_->AddPredictionResult(segment_id, score, clock_.Now());
    segment_selector_->OnModelExecutionCompleted(segment_id);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  TestModelProviderFactory::Data model_providers_;
  TestModelProviderFactory provider_factory_;
  Config config_;
  MockFieldTrialRegister field_trial_register_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  MockSignalStorageConfig signal_storage_config_;
  std::unique_ptr<DefaultModelManager> default_manager_;
  raw_ptr<TestSegmentationResultPrefs> prefs_;
  std::unique_ptr<SegmentSelectorImpl> segment_selector_;
};

TEST_F(SegmentSelectorTest, FindBestSegmentFlowWithTwoSegments) {
  SetUpWithConfig(CreateTestConfig());
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  SegmentId segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(segment_id, mapping, 3);

  SegmentId segment_id2 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
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

TEST_F(SegmentSelectorTest, RunSelectionOnDemand) {
  Config config = CreateTestConfig();
  config.on_demand_execution = true;
  SetUpWithConfig(config);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  static constexpr SegmentId kSegmentId =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(kSegmentId, mapping, 3);

  static constexpr SegmentId kSegmentId2 =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  InitializeMetadataForSegment(kSegmentId2, mapping2, 2);

  auto result_provider = std::make_unique<MockResultProvider>();
  EXPECT_CALL(*result_provider, GetSegmentResult(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          [](std::unique_ptr<SegmentResultProvider::GetResultOptions> options) {
            EXPECT_TRUE(options->ignore_db_scores);
            int rank = options->segment_id == kSegmentId ? 3 : 4;
            auto result =
                std::make_unique<SegmentResultProvider::SegmentResult>(
                    SegmentResultProvider::ResultState::kTfliteModelScoreUsed,
                    rank);
            std::move(options->callback).Run(std::move(result));
          }));
  segment_selector_->set_segment_result_provider_for_testing(
      std::move(result_provider));

  clock_.Advance(base::Days(1));
  base::RunLoop wait_for_selection;
  segment_selector_->GetSelectedSegmentOnDemand(
      /*input_context=*/nullptr,
      base::BindOnce(
          [](base::OnceClosure quit, const SegmentSelectionResult& result) {
            EXPECT_TRUE(result.is_ready);
            EXPECT_EQ(kSegmentId2, *result.segment);
            std::move(quit).Run();
          },
          wait_for_selection.QuitClosure()));
  wait_for_selection.Run();
}

TEST_F(SegmentSelectorTest, NewSegmentResultOverridesThePreviousBest) {
  Config config = CreateTestConfig();
  config.unknown_selection_ttl = base::TimeDelta();
  SetUpWithConfig(config);

  // Setup test with two models.
  SegmentId segment_id1 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}, {0.8, 5}};
  InitializeMetadataForSegment(segment_id1, mapping1, 4);

  SegmentId segment_id2 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping2[][2] = {{0.3, 1}, {0.4, 4}};
  InitializeMetadataForSegment(segment_id2, mapping2, 2);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  // Model 1 completes with a zero-ish score. We will wait for the other model
  // to finish before updating results.
  CompleteModelExecution(segment_id1, 0.1);
  ASSERT_FALSE(prefs_->selection.has_value());

  CompleteModelExecution(segment_id2, 0.1);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Model 1 completes with a good score. Model 2 results are expired.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id1, 0.6);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
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
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID};
  SetUpWithConfig(config);

  SegmentId segment_id =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID;
  float mapping[][2] = {{0.7, 1}};
  InitializeMetadataForSegment(segment_id, mapping, 1);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  // Set a value less than 1 and result should be UNKNOWN.
  CompleteModelExecution(segment_id, 0);
  ASSERT_TRUE(prefs_->selection.has_value());
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Advance by less than UNKNOWN segment TTL and result should not change,
  // UNKNOWN segment TTL is less than selection TTL.
  clock_.Advance(config_.unknown_selection_ttl * 0.8f);
  CompleteModelExecution(segment_id, 0.9);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);

  // Advance clock so that the time is between UNKNOWN segment TTL and selection
  // TTL.
  clock_.Advance(config_.unknown_selection_ttl * 0.4f);
  CompleteModelExecution(segment_id, 0.9);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
            prefs_->selection->segment_id);

  // Advance by more than UNKNOWN segment TTL and result should not change.
  clock_.Advance(config_.unknown_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id, 0);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
            prefs_->selection->segment_id);

  // Advance by segment selection TTL and result should change.
  clock_.Advance(config_.segment_selection_ttl * 1.2f);
  CompleteModelExecution(segment_id, 0);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
            prefs_->selection->segment_id);
}

TEST_F(SegmentSelectorTest, DoesNotMeetSignalCollectionRequirement) {
  SetUpWithConfig(CreateTestConfig());
  SegmentId segment_id1 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}, {0.8, 5}};

  segment_database_->FindOrCreateSegment(segment_id1)
      ->mutable_model_metadata()
      ->set_result_time_to_live(7);
  segment_database_->SetBucketDuration(segment_id1, 1, proto::TimeUnit::DAY);
  segment_database_->AddDiscreteMapping(segment_id1, mapping1, 4,
                                        config_.segmentation_key);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(false));

  CompleteModelExecution(segment_id1, 0.5);
  ASSERT_FALSE(prefs_->selection.has_value());
}

TEST_F(SegmentSelectorTest,
       GetSelectedSegmentReturnsResultFromPreviousSession) {
  SetUpWithConfig(CreateTestConfig());
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));
  SegmentId segment_id0 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  SegmentId segment_id1 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
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
      &config_, &field_trial_register_, &clock_,
      PlatformOptions::CreateDefault(), default_manager_.get());
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
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  SegmentId segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(segment_id, mapping, 3);

  SegmentId segment_id2 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
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

TEST_F(SegmentSelectorTest, SubsegmentRecording) {
  const SegmentId kSubsegmentEnabledTarget =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER;

  // Create config with Feed segment.
  Config config = CreateTestConfig();
  config.segment_ids.push_back(kSubsegmentEnabledTarget);
  // Previous selection result is not available at this time, so it should
  // record unselected.
  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(base::StringPiece("Segmentation_TestKey"),
                                 base::StringPiece("Unselected")));
  SetUpWithConfig(config);

  // Store model metadata, model scores and selection results.
  SegmentId segment_id0 = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  float mapping0[][2] = {{1.0, 0}};
  InitializeMetadataForSegment(segment_id0, mapping0, 1);
  segment_database_->AddPredictionResult(segment_id0, 0.7, clock_.Now());

  float mapping1[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  InitializeMetadataForSegment(kSubsegmentEnabledTarget, mapping1, 3);
  constexpr float kMVTScore = 0.7;
  segment_database_->AddPredictionResult(kSubsegmentEnabledTarget, kMVTScore,
                                         clock_.Now());

  // Additionally store subsegment mapping for Feed segment.
  static constexpr std::array<float[2], 3> kFeedUserScoreToSubGroup = {{
      {1.0, 2},
      {kMVTScore, 3},
      {0.0, 4},
  }};
  segment_database_->AddDiscreteMapping(
      kSubsegmentEnabledTarget, kFeedUserScoreToSubGroup.data(),
      kFeedUserScoreToSubGroup.size(),
      config_.segmentation_key + kSubsegmentDiscreteMappingSuffix);

  // Set up a selected segment in prefs.
  SelectedSegment from_history(segment_id0);
  auto prefs_moved = std::make_unique<TestSegmentationResultPrefs>();
  prefs_ = prefs_moved.get();
  prefs_->selection = from_history;

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(base::StringPiece("Segmentation_TestKey"),
                                 base::StringPiece("Share")));

  // Construct a segment selector. It should read result from last session.
  segment_selector_ = std::make_unique<SegmentSelectorImpl>(
      segment_database_.get(), &signal_storage_config_, std::move(prefs_moved),
      &config_, &field_trial_register_, &clock_,
      PlatformOptions::CreateDefault(), default_manager_.get());

  // When segment result is missing, unknown subsegment is recorded.
  EXPECT_CALL(
      field_trial_register_,
      RegisterSubsegmentFieldTrialIfNeeded(
          base::StringPiece("Segmentation_TestKey_Share"), segment_id0, 0));
  EXPECT_CALL(
      field_trial_register_,
      RegisterSubsegmentFieldTrialIfNeeded(
          base::StringPiece("Segmentation_TestKey_NewTab"),
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 0));

  // The new selector will record subsegment metric groups based on the mapping.
  base::RunLoop wait_for_subsegment;
  EXPECT_CALL(field_trial_register_,
              RegisterSubsegmentFieldTrialIfNeeded(
                  base::StringPiece("Segmentation_TestKey_FeedUserSegment"),
                  kSubsegmentEnabledTarget, 3))
      .WillOnce(
          Invoke([&wait_for_subsegment](base::StringPiece, SegmentId, int) {
            wait_for_subsegment.QuitClosure().Run();
          }));

  segment_selector_->OnPlatformInitialized(nullptr);
  wait_for_subsegment.Run();
}

}  // namespace segmentation_platform
