// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_v2_device_manager_impl.h"

#include "base/optional.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/services/device_sync/fake_cryptauth_device_syncer.h"
#include "chromeos/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakeSessionId[] = "session_id";
const double kFakeLastSuccessTimeSeconds = 1600600000;
constexpr base::TimeDelta kFakeFailureRetryTime =
    base::TimeDelta::FromMinutes(15);

// A FakeCryptAuthScheduler that updates its DeviceSync parameters based on the
// result of the DeviceSync attempt. This makes for reasonable logs.
class FakeCryptAuthSchedulerUpdatedByDeviceSyncResults
    : public FakeCryptAuthScheduler {
  void HandleDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) override {
    FakeCryptAuthScheduler::HandleDeviceSyncResult(device_sync_result);

    if (device_sync_result.IsSuccess()) {
      set_num_consecutive_device_sync_failures(0);
      set_time_to_next_device_sync_request(base::nullopt);
      set_last_successful_device_sync_time(
          base::Time::FromDoubleT(kFakeLastSuccessTimeSeconds));
    } else {
      set_num_consecutive_device_sync_failures(
          GetNumConsecutiveDeviceSyncFailures() + 1);
      set_time_to_next_device_sync_request(kFakeFailureRetryTime);
    }
  }
};

}  // namespace

class DeviceSyncCryptAuthV2DeviceManagerImplTest
    : public testing::Test,
      public CryptAuthV2DeviceManager::Observer {
 protected:
  DeviceSyncCryptAuthV2DeviceManagerImplTest()
      : fake_gcm_manager_(cryptauthv2::kTestGcmRegistrationId),
        mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS) {}

  // testing::Test:
  void SetUp() override {
    CryptAuthDeviceRegistryImpl::RegisterPrefs(test_pref_service_.registry());
    CryptAuthKeyRegistryImpl::RegisterPrefs(test_pref_service_.registry());

    device_registry_ =
        CryptAuthDeviceRegistryImpl::Factory::Get()->BuildInstance(
            &test_pref_service_);
    key_registry_ = CryptAuthKeyRegistryImpl::Factory::Get()->BuildInstance(
        &test_pref_service_);

    fake_device_syncer_factory_ =
        std::make_unique<FakeCryptAuthDeviceSyncerFactory>();
    CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(
        fake_device_syncer_factory_.get());
  }

  // testing::Test:
  void TearDown() override {
    if (device_manager_)
      device_manager_->RemoveObserver(this);

    CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // CryptAuthV2DeviceManager::Observer:
  void OnDeviceSyncStarted(
      const cryptauthv2::ClientMetadata& client_metadata) override {
    client_metadata_sent_to_observer_.push_back(client_metadata);
  }
  void OnDeviceSyncFinished(
      const CryptAuthDeviceSyncResult& device_sync_result) override {
    device_sync_results_sent_to_observer_.push_back(device_sync_result);
  }

  void CreateAndStartDeviceManager() {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    device_manager_ =
        CryptAuthV2DeviceManagerImpl::Factory::Get()->BuildInstance(
            &fake_client_app_metadata_provider_, device_registry_.get(),
            key_registry_.get(), &mock_client_factory_, &fake_gcm_manager_,
            &fake_scheduler_, std::move(mock_timer));

    device_manager_->AddObserver(this);

    EXPECT_FALSE(fake_scheduler_.HasDeviceSyncSchedulingStarted());
    device_manager_->Start();
    EXPECT_TRUE(fake_scheduler_.HasDeviceSyncSchedulingStarted());
  }

  void RequestDeviceSyncThroughSchedulerAndVerify(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) {
    fake_scheduler_.RequestDeviceSync(invocation_reason, session_id);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        invocation_reason, session_id));
  }

  void ForceDeviceSyncRequestAndVerify(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) {
    device_manager_->ForceDeviceSyncNow(invocation_reason, session_id);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        invocation_reason, session_id));
  }

  void RequestDeviceSyncThroughGcmAndVerify(
      const base::Optional<std::string>& session_id) {
    fake_gcm_manager_.PushResyncMessage(session_id,
                                        base::nullopt /* feature_type */);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        cryptauthv2::ClientMetadata::SERVER_INITIATED, session_id));
  }

  void SucceedGetClientAppMetadataRequest() {
    ASSERT_FALSE(
        fake_client_app_metadata_provider_.metadata_requests().empty());
    EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
              fake_client_app_metadata_provider_.metadata_requests()
                  .back()
                  .gcm_registration_id);
    std::move(
        fake_client_app_metadata_provider_.metadata_requests().back().callback)
        .Run(cryptauthv2::GetClientAppMetadataForTest());
  }

  void FailHandleGetClientAppMetadataRequestAndVerifyResult() {
    ASSERT_FALSE(
        fake_client_app_metadata_provider_.metadata_requests().empty());
    EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
              fake_client_app_metadata_provider_.metadata_requests()
                  .back()
                  .gcm_registration_id);
    std::move(
        fake_client_app_metadata_provider_.metadata_requests().back().callback)
        .Run(base::nullopt /* client_app_metadata */);
    ProcessAndVerifyNewDeviceSyncResult(
        CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                      kErrorClientAppMetadataFetchFailed,
                                  false /* did_device_registry_change */,
                                  base::nullopt /* client_directive */));
  }
  void TimeoutWaitingForClientAppMetadataAndVerifyResult() {
    mock_timer_->Fire();
    ProcessAndVerifyNewDeviceSyncResult(
        CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                      kErrorTimeoutWaitingForClientAppMetadata,
                                  false /* did_device_registry_change */,
                                  base::nullopt /* client_directive */));
  }

  void FinishDeviceSyncAttemptAndVerifyResult(
      size_t expected_device_syncer_instance_index,
      const CryptAuthDeviceSyncResult& device_sync_result) {
    EXPECT_TRUE(device_manager_->IsDeviceSyncInProgress());

    // A valid GCM registration ID and valid ClientAppMetadata must exist before
    // the device syncer can be invoked.
    EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
              fake_gcm_manager_.GetRegistrationId());
    ASSERT_FALSE(
        fake_client_app_metadata_provider_.metadata_requests().empty());

    // Only the most recently created device syncer is valid.
    EXPECT_EQ(fake_device_syncer_factory_->instances().size() - 1,
              expected_device_syncer_instance_index);
    FakeCryptAuthDeviceSyncer* device_syncer =
        fake_device_syncer_factory_
            ->instances()[expected_device_syncer_instance_index];

    VerifyDeviceSyncerData(device_syncer,
                           expected_client_metadata_list_.back());

    device_syncer->FinishAttempt(device_sync_result);
    EXPECT_FALSE(device_manager_->IsDeviceSyncInProgress());

    ProcessAndVerifyNewDeviceSyncResult(device_sync_result);
  }

  CryptAuthV2DeviceManager* device_manager() { return device_manager_.get(); }

 private:
  // Adds the ClientMetadata from the latest DeviceSync request to a list of
  // expected ClientMetadata from all DeviceSync requests. Verifies that this
  // ClientMetadata is communicated to the device manager's observers.
  void ProcessAndVerifyNewDeviceSyncRequest(
      const cryptauthv2::ClientMetadata& expected_client_metadata) {
    expected_client_metadata_list_.push_back(expected_client_metadata);

    VerifyDeviceManagerObserversNotifiedOfStart(expected_client_metadata_list_);
  }

  void VerifyDeviceManagerObserversNotifiedOfStart(
      const std::vector<cryptauthv2::ClientMetadata>&
          expected_client_metadata_list) {
    ASSERT_EQ(expected_client_metadata_list.size(),
              client_metadata_sent_to_observer_.size());
    for (size_t i = 0; i < expected_client_metadata_list.size(); ++i) {
      EXPECT_EQ(expected_client_metadata_list[i].SerializeAsString(),
                client_metadata_sent_to_observer_[i].SerializeAsString());
    }
  }

  // Verifies the input to the device syncer.
  void VerifyDeviceSyncerData(
      FakeCryptAuthDeviceSyncer* device_syncer,
      const cryptauthv2::ClientMetadata& expected_client_metadata) {
    ASSERT_TRUE(device_syncer->client_metadata());
    ASSERT_TRUE(device_syncer->client_app_metadata());
    EXPECT_EQ(expected_client_metadata.SerializeAsString(),
              device_syncer->client_metadata()->SerializeAsString());
    EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().SerializeAsString(),
              device_syncer->client_app_metadata()->SerializeAsString());
  }

  // Adds the result of the latest DeviceSync attempt to a list of all expected
  // DeviceSync results. Verifies that the results are communicated to the
  // device manager's observers and the scheduler.
  void ProcessAndVerifyNewDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) {
    expected_device_sync_results_.push_back(device_sync_result);

    EXPECT_EQ(expected_device_sync_results_,
              device_sync_results_sent_to_observer_);
    EXPECT_EQ(expected_device_sync_results_,
              fake_scheduler_.handled_device_sync_results());
  }

  std::vector<cryptauthv2::ClientMetadata> expected_client_metadata_list_;
  std::vector<CryptAuthDeviceSyncResult> expected_device_sync_results_;

  std::vector<cryptauthv2::ClientMetadata> client_metadata_sent_to_observer_;
  std::vector<CryptAuthDeviceSyncResult> device_sync_results_sent_to_observer_;

  TestingPrefServiceSimple test_pref_service_;
  FakeClientAppMetadataProvider fake_client_app_metadata_provider_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  FakeCryptAuthSchedulerUpdatedByDeviceSyncResults fake_scheduler_;
  base::MockOneShotTimer* mock_timer_ = nullptr;
  MockCryptAuthClientFactory mock_client_factory_;
  std::unique_ptr<CryptAuthDeviceRegistry> device_registry_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<FakeCryptAuthDeviceSyncerFactory> fake_device_syncer_factory_;

  std::unique_ptr<CryptAuthV2DeviceManager> device_manager_;
};

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest,
       RequestDeviceSyncThroughScheduler) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::INITIALIZATION,
      base::nullopt /* session_id */);
  SucceedGetClientAppMetadataRequest();
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* did_device_registry_change */,
                                cryptauthv2::GetClientDirectiveForTest()));
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest, ForceDeviceSync_Success) {
  CreateAndStartDeviceManager();
  ForceDeviceSyncRequestAndVerify(cryptauthv2::ClientMetadata::MANUAL,
                                  kFakeSessionId);
  SucceedGetClientAppMetadataRequest();
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* did_device_registry_change */,
                                cryptauthv2::GetClientDirectiveForTest()));
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest,
       RequestDeviceSyncThroughGcm) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughGcmAndVerify(kFakeSessionId);
  SucceedGetClientAppMetadataRequest();
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* did_device_registry_change */,
                                cryptauthv2::GetClientDirectiveForTest()));
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest, FailureRetry) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, base::nullopt /* session_id */);
  SucceedGetClientAppMetadataRequest();

  // Fail first attempt with fatal error.
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::
              kErrorSyncMetadataApiCallInternalServerError,
          false /* did_device_registry_change */,
          base::nullopt /* client_directive */));
  EXPECT_EQ(kFakeFailureRetryTime, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(base::nullopt, device_manager()->GetLastDeviceSyncTime());
  EXPECT_TRUE(device_manager()->IsRecoveringFromFailure());

  // Fail second attempt with non-fatal error.
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, kFakeSessionId);
  FinishDeviceSyncAttemptAndVerifyResult(
      1u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          false /* did_device_registry_change */,
          base::nullopt /* client_directive */));
  EXPECT_EQ(kFakeFailureRetryTime, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(base::nullopt, device_manager()->GetLastDeviceSyncTime());
  EXPECT_TRUE(device_manager()->IsRecoveringFromFailure());

  // Succeed third attempt.
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, kFakeSessionId);
  FinishDeviceSyncAttemptAndVerifyResult(
      2u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kSuccess,
          true /* did_device_registry_change */,
          cryptauthv2::GetClientDirectiveForTest() /* client_directive */));
  EXPECT_EQ(base::nullopt, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(base::Time::FromDoubleT(kFakeLastSuccessTimeSeconds),
            device_manager()->GetLastDeviceSyncTime());
  EXPECT_FALSE(device_manager()->IsRecoveringFromFailure());
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest,
       ClientAppMetadataFetch_Failure) {
  CreateAndStartDeviceManager();

  // Fail to fetch ClientAppMetadata first attempt.
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, base::nullopt /* session_id */);
  FailHandleGetClientAppMetadataRequestAndVerifyResult();

  // Succeed ClientAppMetadata fetch second attempt.
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, base::nullopt /* session_id */);
  SucceedGetClientAppMetadataRequest();
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* did_device_registry_change */,
                                cryptauthv2::GetClientDirectiveForTest()));
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest,
       ClientAppMetadataFetch_Timeout) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, base::nullopt /* session_id */);
  TimeoutWaitingForClientAppMetadataAndVerifyResult();
}

}  // namespace device_sync

}  // namespace chromeos
