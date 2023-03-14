// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::NiceMock;

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
}  // namespace

class DeviceSwitcherResultDispatcherTest : public testing::Test {
 public:
  DeviceSwitcherResultDispatcherTest() = default;
  ~DeviceSwitcherResultDispatcherTest() override = default;

  void SetUp() override {
    Test::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    DeviceSwitcherResultDispatcher::RegisterProfilePrefs(prefs_->registry());
    sync_service_ = std::make_unique<syncer::TestSyncService>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    Test::TearDown();
    histogram_tester_.reset();
    prefs_.reset();
    sync_service_.reset();
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& expected,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected.ordered_labels, actual.ordered_labels);
    EXPECT_EQ(expected.status, actual.status);
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<MockSegmentationPlatformService> segmentation_platform_service_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  NiceMock<MockFieldTrialRegister> field_trial_register_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(DeviceSwitcherResultDispatcherTest, SegmentationFailed) {
  ClassificationResult result(PredictionStatus::kFailed);

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(result));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, base::StringPiece("Unselected")));

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, sync_service_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
}

TEST_F(DeviceSwitcherResultDispatcherTest, TestWaitForClassificationResult) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label1");
  sync_service_->SetHasSyncConsent(true);

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(result));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, base::StringPiece("test_label1")));

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, sync_service_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();

  histogram_tester_->ExpectTotalCount(
      "SegmentationPlatform.DeviceSwicther.TimeFromStartupToResult", 1);
}

TEST_F(DeviceSwitcherResultDispatcherTest, ResultRefreshedOnSyncConsent) {
  // Create a classification result.
  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back(DeviceSwitcherModel::kNotSyncedLabel);
  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back(DeviceSwitcherModel::kAndroidPhoneLabel);
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(result1))
      .WillOnce(RunOnceCallback<3>(result2));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(
                  _, base::StringPiece(DeviceSwitcherModel::kNotSyncedLabel)));
  EXPECT_CALL(
      field_trial_register_,
      RegisterFieldTrial(
          _, base::StringPiece(DeviceSwitcherModel::kAndroidPhoneLabel)));

  sync_service_->SetHasSyncConsent(false);

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, sync_service_.get(), prefs_.get(),
      &field_trial_register_);
  base::RunLoop().RunUntilIdle();

  sync_service_->SetHasSyncConsent(true);
  sync_service_->FireStateChanged();
  base::RunLoop().RunUntilIdle();

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result2));
  loop.Run();
}

TEST_F(DeviceSwitcherResultDispatcherTest, TestGetCachedClassificationResult) {
  // Create initial and updated classification results.
  ClassificationResult expected_initial_result(PredictionStatus::kSucceeded);
  expected_initial_result.ordered_labels.emplace_back(
      DeviceSwitcherModel::kNotSyncedLabel);
  ClassificationResult expected_updated_result(PredictionStatus::kSucceeded);
  expected_updated_result.ordered_labels.emplace_back("test_label1");

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(expected_initial_result))
      .WillOnce(RunOnceCallback<3>(expected_updated_result));

  // Setup DeviceSwitcherResultDispatcher; sync consent will be updated during
  // test execution to verify cached result update.
  sync_service_->SetHasSyncConsent(false);
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, sync_service_.get(), prefs_.get(),
      &field_trial_register_);

  ClassificationResult actual_initial_result =
      device_switcher_result_dispatcher.GetCachedClassificationResult();
  ASSERT_EQ(actual_initial_result.ordered_labels[0],
            DeviceSwitcherModel::kNotSyncedLabel);
  sync_service_->SetHasSyncConsent(true);
  sync_service_->FireStateChanged();
  ClassificationResult actual_updated_result =
      device_switcher_result_dispatcher.GetCachedClassificationResult();
  EXPECT_EQ(actual_updated_result.status, PredictionStatus::kSucceeded);
  ASSERT_EQ(actual_updated_result.ordered_labels.size(), 1U);
  EXPECT_EQ(actual_updated_result.ordered_labels[0], "test_label1");
}

TEST_F(DeviceSwitcherResultDispatcherTest,
       TestGetClassificationResultAfterWaiting) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label1");
  sync_service_->SetHasSyncConsent(false);

  // Save the callback to simulate a delayed result.
  ClassificationResultCallback callback;
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillRepeatedly(MoveArg<3>(&callback));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, base::StringPiece("test_label1")));

  // The DeviceSwitcherResultDispatcher will wait for the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, sync_service_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result));
  sync_service_->SetHasSyncConsent(true);
  sync_service_->FireStateChanged();
  std::move(callback).Run(result);
  loop.Run();

  histogram_tester_->ExpectTotalCount(
      "SegmentationPlatform.DeviceSwicther.TimeFromConsentToResult", 1);
}

}  // namespace segmentation_platform
