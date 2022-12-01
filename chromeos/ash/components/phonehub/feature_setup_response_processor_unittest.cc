// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/phonehub/feature_setup_response_processor.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/combined_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/fake_message_receiver.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

class FakeCombinedAccessSetupOperationDelegate
    : public CombinedAccessSetupOperation::Delegate {
 public:
  FakeCombinedAccessSetupOperationDelegate() = default;
  ~FakeCombinedAccessSetupOperationDelegate() override = default;

  CombinedAccessSetupOperation::Status status() const { return status_; }

  // CombinedAccessSetupOperation::Delegate:
  void OnCombinedStatusChange(
      CombinedAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  CombinedAccessSetupOperation::Status status_ =
      CombinedAccessSetupOperation::Status::kConnecting;
};

}  // namespace

class FeatureSetupResponseProcessorTest : public testing::Test {
 protected:
  FeatureSetupResponseProcessorTest() = default;
  FeatureSetupResponseProcessorTest(const FeatureSetupResponseProcessorTest&) =
      delete;
  FeatureSetupResponseProcessorTest& operator=(
      const FeatureSetupResponseProcessorTest&) = delete;
  ~FeatureSetupResponseProcessorTest() override = default;

  void SetUp() override {
    fake_message_receiver_ = std::make_unique<FakeMessageReceiver>();
    fake_multidevice_feature_access_manager_ =
        std::make_unique<FakeMultideviceFeatureAccessManager>();
    fake_multidevice_feature_access_manager_
        ->SetFeatureSetupRequestSupportedInternal(true);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                              features::kPhoneHubFeatureSetupErrorHandling},
        /*disabled_features=*/{});
  }

  void CreateFeatureSetupResponseProcessor() {
    feature_setup_response_processor_ =
        std::make_unique<FeatureSetupResponseProcessor>(
            fake_message_receiver_.get(),
            fake_multidevice_feature_access_manager_.get());
  }

  CombinedAccessSetupOperation::Status GetCombinedSetupOperationStatus() {
    return fake_combined_delegate_.status();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeMessageReceiver> fake_message_receiver_;
  std::unique_ptr<FakeMultideviceFeatureAccessManager>
      fake_multidevice_feature_access_manager_;
  std::unique_ptr<FeatureSetupResponseProcessor>
      feature_setup_response_processor_;
  FakeCombinedAccessSetupOperationDelegate fake_combined_delegate_;
};
TEST_F(FeatureSetupResponseProcessorTest, ResponseReceived_Not_In_Setup) {
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);

  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  // Should not be updated.
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kConnecting,
            GetCombinedSetupOperationStatus());
}

TEST_F(FeatureSetupResponseProcessorTest, ResponseReceived_All_Access_Granted) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  // Success cases should not be handled by this processor
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kConnecting,
            GetCombinedSetupOperationStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Access_Declined) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(
      CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess,
      GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Requested_Notification_Access_Decliend) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(CombinedAccessSetupOperation::Status::
                kCameraRollGrantedNotificationRejected,
            GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Requested_CameraRoll_Access_Decliend) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(CombinedAccessSetupOperation::Status::
                kCameraRollRejectedNotificationGranted,
            GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_CameraRoll_Requested_Access_Decliend) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, false, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(
      CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess,
      GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_Notification_Requested_Access_Decliend) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          false, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(
      CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess,
      GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Requested_CameraRoll_Setup_Interrupted) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_ACTION_CANCELED);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_ACTION_CANCELED);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Requested_Notification_Setup_Interrupted) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_ACTION_CANCELED);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

TEST_F(FeatureSetupResponseProcessorTest,
       ResponseReceived_All_Requested_Notification_Setup_Timeout) {
  auto operation =
      fake_multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          true, true, &fake_combined_delegate_);
  EXPECT_TRUE(operation);
  CreateFeatureSetupResponseProcessor();
  proto::FeatureSetupResponse setupResponse;
  setupResponse.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT);
  setupResponse.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_ERROR_ACTION_TIMEOUT);

  fake_message_receiver_->NotifyFeatureSetupResponseReceived(setupResponse);

  EXPECT_EQ(CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetCombinedSetupOperationStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->IsCombinedSetupOperationInProgress());
}

}  // namespace phonehub
}  // namespace ash
