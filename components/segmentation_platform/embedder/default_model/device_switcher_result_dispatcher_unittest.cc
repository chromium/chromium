// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using base::test::RunOnceCallback;
using syncer::DeviceInfo;
using testing::_;
using testing::NiceMock;
using OsType = syncer::DeviceInfo::OsType;
using DeviceCountByOsTypeMap = std::map<OsType, int>;
using syncer::FakeDeviceInfoTracker;
using DeviceType = sync_pb::SyncEnums::DeviceType;
using DeviceCountByOsTypeMap = std::map<DeviceInfo::OsType, int>;

const sync_pb::SyncEnums_DeviceType kLocalDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
const DeviceInfo::OsType kLocalDeviceOS = DeviceInfo::OsType::kLinux;
const DeviceInfo::FormFactor kLocalDeviceFormFactor =
    DeviceInfo::FormFactor::kDesktop;

std::unique_ptr<DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    DeviceType device_type,
    OsType os_type,
    base::Time last_updated = base::Time::Now()) {
  return std::make_unique<DeviceInfo>(
      guid, "name", "chrome_version", "user_agent", device_type, os_type,
      kLocalDeviceFormFactor, "device_id", "manufacturer_name", "model_name",
      "full_hardware_class", last_updated,
      syncer::DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);
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

class DeviceSwitcherResultDispatcherTest : public testing::Test {
 public:
  DeviceSwitcherResultDispatcherTest() = default;
  ~DeviceSwitcherResultDispatcherTest() override = default;

  void SetUp() override {
    Test::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    DeviceSwitcherResultDispatcher::RegisterProfilePrefs(prefs_->registry());
    device_info_tracker_ = std::make_unique<FakeDeviceInfoTracker>();
  }

  void TearDown() override {
    Test::TearDown();
    prefs_.reset();
    if (local_device_info_) {
      device_info_tracker_->Remove(local_device_info_.get());
      local_device_info_.reset();
    }
    device_info_tracker_.reset();
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& expected,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected.ordered_labels, actual.ordered_labels);
    EXPECT_EQ(expected.status, actual.status);
    std::move(closure).Run();
  }

  void MakeDeviceInfoAvailable() {
    ASSERT_FALSE(local_device_info_);
    local_device_info_ =
        CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
    device_info_tracker_->Add(local_device_info_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockSegmentationPlatformService> segmentation_platform_service_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  NiceMock<MockFieldTrialRegister> field_trial_register_;
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<DeviceInfo> local_device_info_;
};

TEST_F(DeviceSwitcherResultDispatcherTest, SegmentationFailed) {
  ClassificationResult result(PredictionStatus::kFailed);

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(result));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, std::string_view("Unselected")));

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(
      base::Seconds(5),
      base::BindOnce(
          &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
}

TEST_F(DeviceSwitcherResultDispatcherTest, TestWaitForClassificationResult) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label1");

  MakeDeviceInfoAvailable();

  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(result));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, std::string_view("test_label1")));

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(
      base::Seconds(5),
      base::BindOnce(
          &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
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
                  _, std::string_view(DeviceSwitcherModel::kNotSyncedLabel)));
  EXPECT_CALL(
      field_trial_register_,
      RegisterFieldTrial(
          _, std::string_view(DeviceSwitcherModel::kAndroidPhoneLabel)));

  // The DeviceSwitcherResultDispatcher will find the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);
  base::RunLoop().RunUntilIdle();

  MakeDeviceInfoAvailable();

  base::RunLoop().RunUntilIdle();

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(
      base::Seconds(5),
      base::BindOnce(
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
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);

  ClassificationResult actual_initial_result =
      device_switcher_result_dispatcher.GetCachedClassificationResult();
  ASSERT_EQ(actual_initial_result.ordered_labels[0],
            DeviceSwitcherModel::kNotSyncedLabel);
  MakeDeviceInfoAvailable();
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

  // Save the callback to simulate a delayed result.
  ClassificationResultCallback callback;
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillRepeatedly(MoveArg<3>(&callback));

  EXPECT_CALL(field_trial_register_,
              RegisterFieldTrial(_, std::string_view("test_label1")));

  // The DeviceSwitcherResultDispatcher will wait for the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(
      base::Seconds(5),
      base::BindOnce(
          &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure(), result));

  MakeDeviceInfoAvailable();
  std::move(callback).Run(result);
  loop.Run();
}

TEST_F(DeviceSwitcherResultDispatcherTest,
       TestGetClassificationResultWaitTimeout) {
  // Create a classification result.
  ClassificationResult result(PredictionStatus::kNotReady);

  // Save the callback to simulate a delayed result.
  ClassificationResultCallback callback;
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillRepeatedly(MoveArg<3>(&callback));

  // The DeviceSwitcherResultDispatcher will wait for the result returned by the
  // segmentation platform service.
  DeviceSwitcherResultDispatcher device_switcher_result_dispatcher(
      &segmentation_platform_service_, device_info_tracker_.get(), prefs_.get(),
      &field_trial_register_);

  base::RunLoop loop;
  device_switcher_result_dispatcher.WaitForClassificationResult(
      base::Seconds(5),
      base::BindOnce(
          &DeviceSwitcherResultDispatcherTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure(), result));

  task_environment_.FastForwardBy(base::Seconds(5));
  loop.Run();
}

}  // namespace segmentation_platform
