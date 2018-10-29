// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/device_reenroller.h"

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "components/cryptauth/fake_gcm_device_info_provider.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupDeviceReenrollerTest : public testing::Test {
 protected:
  MultiDeviceSetupDeviceReenrollerTest()
      : test_local_device_(cryptauth::CreateRemoteDeviceRefForTest()) {}
  ~MultiDeviceSetupDeviceReenrollerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->NotifyReady();

    fake_gcm_device_info_provider_ =
        std::make_unique<cryptauth::FakeGcmDeviceInfoProvider>(
            cryptauth::GcmDeviceInfo());
  }

  void SetLocalDeviceMetadataSoftwareFeaturesMap(
      const std::map<cryptauth::SoftwareFeature,
                     cryptauth::SoftwareFeatureState>& map) {
    cryptauth::GetMutableRemoteDevice(test_local_device_)->software_features =
        map;
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
  }

  void SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      const std::vector<cryptauth::SoftwareFeature>&
          supported_software_features) {
    cryptauth::GcmDeviceInfo gcm_device_info;
    gcm_device_info.clear_supported_software_features();
    for (cryptauth::SoftwareFeature feature : supported_software_features) {
      gcm_device_info.add_supported_software_features(feature);
    }
    fake_gcm_device_info_provider_ =
        std::make_unique<cryptauth::FakeGcmDeviceInfoProvider>(gcm_device_info);
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  base::MockOneShotTimer* timer() { return mock_timer_; }

  void CreateDeviceReenroller() {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    device_reenroller_ = DeviceReenroller::Factory::Get()->BuildInstance(
        fake_device_sync_client_.get(), fake_gcm_device_info_provider_.get(),
        std::move(mock_timer));
  }

  // After a successful re-enrollment and device sync, there should be a timer
  // running to confirm that everything worked as expected.
  void FireTimerAndVerifyResults() {
    // Check-up timer should be running.
    EXPECT_TRUE(timer()->IsRunning());
    // Check should now pass with no further action taken.
    timer()->Fire();
    EXPECT_EQ(
        0, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
    EXPECT_EQ(0, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
    EXPECT_FALSE(timer()->IsRunning());
  }

 private:
  cryptauth::RemoteDeviceRef test_local_device_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<cryptauth::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;
  base::MockOneShotTimer* mock_timer_;

  std::unique_ptr<DeviceReenroller> device_reenroller_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupDeviceReenrollerTest);
};

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfGmcDeviceInfoAndLocalDeviceMetadataMatchThenNoReenrollment) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Set the current GcmDeviceInfo supported software features to contain the
  // same set.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT});

  CreateDeviceReenroller();

  // No enrollment or device sync attempts should have taken place nor should
  // any be scheduled.
  EXPECT_EQ(
      0, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_EQ(0, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfFeaturesBecomeUnsupportedThenUpdateAndReenroll) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Remove one supported software feature in the GcmDeviceInfo.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT});

  CreateDeviceReenroller();

  // Assume successful enrollment, sync, and local device metadata update.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_TRUE(timer()->IsRunning());
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyEnrollmentFinished();
  EXPECT_EQ(1, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  fake_device_sync_client()->InvokePendingForceSyncNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  FireTimerAndVerifyResults();
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfFeaturesBecomeSupportedThenUpdateAndReenroll) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  // Assume successful enrollment, sync, and local device metadata update.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_TRUE(timer()->IsRunning());
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyEnrollmentFinished();
  EXPECT_EQ(1, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  fake_device_sync_client()->InvokePendingForceSyncNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  FireTimerAndVerifyResults();
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfEnrollmentCallFailsThenAnotherAttemptShouldBeScheduled) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo to trigger a
  // re-enrollment attempt.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  // Assume unsuccessful enrollment call.
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      false /* success */);
  // Another re-enrollment attempt should be scheduled.
  EXPECT_TRUE(timer()->IsRunning());
  // This should trigger another enrollment attempt.
  timer()->Fire();
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfDeviceSyncCallFailsThenAnotherEnrollmentAttemptShouldBeScheduled) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo to trigger a
  // re-enrollment attempt.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  // Assume successful enrollment attempt.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyEnrollmentFinished();
  EXPECT_EQ(1, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  // Assume unsuccessful device sync call.
  fake_device_sync_client()->InvokePendingForceSyncNowCallback(
      false /* success */);
  // Another re-enrollment attempt should be scheduled.
  EXPECT_TRUE(timer()->IsRunning());
  // This should trigger another enrollment attempt.
  timer()->Fire();
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfMetadataNotUpdatedCorrectlyThenAnotherEnrollAttemptShouldBeScheduled) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo to trigger a
  // re-enrollment attempt.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  // Assume successful enrollment and device sync.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyEnrollmentFinished();
  EXPECT_EQ(1, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  // Assume local device metadata was not updated correctly.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  fake_device_sync_client()->InvokePendingForceSyncNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyNewDevicesSynced();
  // Another enrollment attempt should be scheduled.
  EXPECT_TRUE(timer()->IsRunning());
  // This should trigger another enrollment attempt.
  timer()->Fire();
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       GcmDeviceInfoFeatureListOrderingAndDuplicatesAreIrrelevantForReenroll) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT,
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  // Assume successful enrollment, sync, and local device metadata update.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  fake_device_sync_client()->InvokePendingForceEnrollmentNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyEnrollmentFinished();
  EXPECT_EQ(1, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  fake_device_sync_client()->InvokePendingForceSyncNowCallback(
      true /* success */);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  FireTimerAndVerifyResults();
}

TEST_F(
    MultiDeviceSetupDeviceReenrollerTest,
    GcmDeviceInfoFeatureListOrderingAndDuplicatesAreIrrelevantForNoReenroll) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Add one more supported software feature in the GcmDeviceInfo.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT});

  CreateDeviceReenroller();

  EXPECT_EQ(
      0, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_EQ(0, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  // No other attempts should be scheduled.
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfOnEnrollmentFinishedCalledWithAgreementThenNoReenrollment) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Set the current GcmDeviceInfo supported software features to contain the
  // same set.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT});

  CreateDeviceReenroller();

  fake_device_sync_client()->NotifyEnrollmentFinished();

  // No enrollment or device sync attempts should have taken place nor should
  // any be scheduled.
  EXPECT_EQ(
      0, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_EQ(0, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfOnNewDevicesSyncedCalledWithAgreementThenNoReenrollment) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Set the current GcmDeviceInfo supported software features to contain the
  // same set.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT});

  CreateDeviceReenroller();

  fake_device_sync_client()->NotifyNewDevicesSynced();

  // No enrollment or device sync attempts should have taken place nor should
  // any be scheduled.
  EXPECT_EQ(
      0, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_EQ(0, fake_device_sync_client()->GetForceSyncNowCallbackQueueSize());
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(MultiDeviceSetupDeviceReenrollerTest,
       IfOnNewDevicesSyncedCalledWithDisagreementThenStartReenrollment) {
  // Set the current local device metadata to contain a sample of supported
  // software features.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported},
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});
  // Set the current GcmDeviceInfo supported software features to contain the
  // same set.
  SetFakeGcmDeviceInfoProviderWithSupportedSoftwareFeatures(
      std::vector<cryptauth::SoftwareFeature>{
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT});

  CreateDeviceReenroller();

  EXPECT_FALSE(timer()->IsRunning());

  // Remove a feature from the metadata.
  SetLocalDeviceMetadataSoftwareFeaturesMap(
      std::map<cryptauth::SoftwareFeature, cryptauth::SoftwareFeatureState>{
          {cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
           cryptauth::SoftwareFeatureState::kSupported}});

  fake_device_sync_client()->NotifyNewDevicesSynced();

  // Start re-enrollment process.
  EXPECT_EQ(
      1, fake_device_sync_client()->GetForceEnrollmentNowCallbackQueueSize());
  EXPECT_TRUE(timer()->IsRunning());
}

}  // namespace multidevice_setup

}  // namespace chromeos
