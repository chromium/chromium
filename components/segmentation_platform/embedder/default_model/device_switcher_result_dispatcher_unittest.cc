// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
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
    DeviceSwitcherResultDispatcher::RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override { Test::TearDown(); }

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
  TestingPrefServiceSimple prefs_;
  NiceMock<MockFieldTrialRegister> field_trial_register_;
};

TEST_F(DeviceSwitcherResultDispatcherTest, TestGetClassificationResult) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label1");

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .Times(1)
      .WillRepeatedly(RunOnceCallback<3>(result));

  EXPECT_CALL(field_trial_register_, RegisterFieldTrial(_, _)).Times(1);

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, &prefs_, &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.GetClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
}

TEST_F(DeviceSwitcherResultDispatcherTest,
       TestGetClassificationResultAfterWaiting) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label1");

  // Save the callback to simulate a delayed result.
  ClassificationResultCallback callback;
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(MoveArg<3>(&callback));

  EXPECT_CALL(field_trial_register_, RegisterFieldTrial(_, _)).Times(1);

  // The DeviceSwitcherResultDispatcher will wait for the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, &prefs_, &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.GetClassificationResult(base::BindOnce(
      &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
      base::Unretained(this), loop.QuitClosure(), result));
  std::move(callback).Run(result);
  loop.Run();
}

}  // namespace segmentation_platform
