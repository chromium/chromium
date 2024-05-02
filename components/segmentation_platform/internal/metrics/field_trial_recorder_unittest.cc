// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metrics/field_trial_recorder.h"

#include <string_view>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using ::testing::NiceMock;

proto::ClientResult CreateClientResult(proto::PredictionResult pred_result) {
  proto::ClientResult client_result;
  client_result.mutable_client_result()->CopyFrom(pred_result);
  client_result.set_timestamp_us(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return client_result;
}

class MockFieldTrialRegister : public FieldTrialRegister {
 public:
  MOCK_METHOD2(RegisterFieldTrial,
               void(std::string_view trial_name, std::string_view group_name));

  MOCK_METHOD3(RegisterSubsegmentFieldTrialIfNeeded,
               void(std::string_view trial_name,
                    proto::SegmentId segment_id,
                    int subsegment_rank));
};
}  // namespace

class FieldTrialRecorderTest : public testing::Test {
 public:
  FieldTrialRecorderTest() = default;
  ~FieldTrialRecorderTest() override = default;

  void SetUp() override {
    result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service_);
    pref_service_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                 std::string());

    field_trial_recorder_ =
        std::make_unique<FieldTrialRecorder>(&field_trial_register_);
    configs_.push_back(test_utils::CreateTestConfig(
        "test_key",
        proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER));
  }

 protected:
  NiceMock<MockFieldTrialRegister> field_trial_register_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  std::unique_ptr<FieldTrialRecorder> field_trial_recorder_;
  std::unique_ptr<CachedResultProvider> cached_result_provider_;
  std::vector<std::unique_ptr<Config>> configs_;
};

TEST_F(FieldTrialRecorderTest, RecordUnselectedFieldTrial) {
  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(std::string_view("Segmentation_test_key"),
                                 std::string_view("Unselected")));

  field_trial_recorder_->RecordFieldTrialAtStartup(
      configs_, cached_result_provider_.get());
}

TEST_F(FieldTrialRecorderTest, RecordFieldTrial) {
  result_prefs_->SaveClientResultToPrefs(
      "test_key", CreateClientResult(metadata_utils::CreatePredictionResult(
                      /*model_scores=*/{0.8},
                      test_utils::GetTestOutputConfigForBinnedClassifier(),
                      /*timestamp=*/base::Time::Now(), /*model_version=*/1)));
  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(std::string_view("Segmentation_test_key"),
                                 std::string_view("High")));

  field_trial_recorder_->RecordFieldTrialAtStartup(
      configs_, cached_result_provider_.get());
}

TEST_F(FieldTrialRecorderTest, RecordFieldTrialForNonClassification) {
  result_prefs_->SaveClientResultToPrefs(
      "test_key",
      CreateClientResult(metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8},
          test_utils::GetTestOutputConfigForGenericPredictor({"label1"}),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1)));
  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(std::string_view("Segmentation_test_key"),
                                 std::string_view("Unselected")));

  field_trial_recorder_->RecordFieldTrialAtStartup(
      configs_, cached_result_provider_.get());
}

}  // namespace segmentation_platform
