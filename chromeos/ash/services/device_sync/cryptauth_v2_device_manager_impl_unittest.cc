// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager_impl.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/fake_attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_device_syncer.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/fake_synced_bluetooth_address_tracker.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeSessionId[] = "session_id";
const double kFakeLastSuccessTimeSeconds = 1600600000;
constexpr base::TimeDelta kFakeFailureRetryTime = base::Minutes(15);

// A FakeCryptAuthScheduler that updates its DeviceSync parameters based on the
// result of the DeviceSync attempt. This makes for reasonable logs.
class FakeCryptAuthSchedulerUpdatedByDeviceSyncResults
    : public FakeCryptAuthScheduler {
  void HandleDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) override {
    FakeCryptAuthScheduler::HandleDeviceSyncResult(device_sync_result);

    if (device_sync_result.IsSuccess()) {
      set_num_consecutive_device_sync_failures(0);
      set_time_to_next_device_sync_request(std::nullopt);
      set_last_successful_device_sync_time(
          base::Time::FromSecondsSinceUnixEpoch(kFakeLastSuccessTimeSeconds));
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
        CryptAuthDeviceRegistryImpl::Factory::Create(&test_pref_service_);
    key_registry_ =
        CryptAuthKeyRegistryImpl::Factory::Create(&test_pref_service_);

    fake_device_syncer_factory_ =
        std::make_unique<FakeCryptAuthDeviceSyncerFactory>();
    CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(
        fake_device_syncer_factory_.get());

    fake_attestation_certificates_syncer_factory_ =
        std::make_unique<FakeAttestationCertificatesSyncerFactory>();
    AttestationCertificatesSyncerImpl::Factory::SetFactoryForTesting(
        fake_attestation_certificates_syncer_factory_.get());

    SyncedBluetoothAddressTrackerImpl::Factory::SetFactoryForTesting(
        &fake_synced_bluetooth_address_tracker_factory_);
  }

  // testing::Test:
  void TearDown() override {
    if (device_manager_)
      device_manager_->RemoveObserver(this);

    CryptAuthDeviceSyncerImpl::Factory::SetFactoryForTesting(nullptr);
    AttestationCertificatesSyncerImpl::Factory::SetFactoryForTesting(nullptr);
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
    device_manager_ = CryptAuthV2DeviceManagerImpl::Factory::Create(
        cryptauthv2::GetClientAppMetadataForTest(), device_registry_.get(),
        key_registry_.get(), &mock_client_factory_, &fake_gcm_manager_,
        &fake_scheduler_, &test_pref_service_,
        base::BindRepeating(
            [](AttestationCertificatesSyncer::NotifyCallback notifyCallback,
               const std::string&) {}));

    device_manager_->AddObserver(this);

    EXPECT_FALSE(fake_scheduler_.HasDeviceSyncSchedulingStarted());
    device_manager_->Start();
    EXPECT_TRUE(fake_scheduler_.HasDeviceSyncSchedulingStarted());
  }

  void RequestDeviceSyncThroughSchedulerAndVerify(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) {
    fake_scheduler_.RequestDeviceSync(invocation_reason, session_id);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        invocation_reason, session_id));
  }

  void ForceDeviceSyncRequestAndVerify(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) {
    device_manager_->ForceDeviceSyncNow(invocation_reason, session_id);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        invocation_reason, session_id));
  }

  void RequestDeviceSyncThroughGcmAndVerify(
      const std::optional<std::string>& session_id) {
    fake_gcm_manager_.PushResyncMessage(session_id,
                                        std::nullopt /* feature_type */);

    ProcessAndVerifyNewDeviceSyncRequest(cryptauthv2::BuildClientMetadata(
        fake_scheduler_.GetNumConsecutiveDeviceSyncFailures(),
        cryptauthv2::ClientMetadata::SERVER_INITIATED, session_id));
  }

  void FinishDeviceSyncAttemptAndVerifyResult(
      size_t expected_device_syncer_instance_index,
      const CryptAuthDeviceSyncResult& device_sync_result) {
    EXPECT_TRUE(device_manager_->IsDeviceSyncInProgress());

    // A valid GCM registration ID and valid ClientAppMetadata must exist before
    // the device syncer can be invoked.
    EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
              fake_gcm_manager_.GetRegistrationId());

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
  // ClientMetadata is communicated to the device manager's observers and to
  // the metrics loggers.
  void ProcessAndVerifyNewDeviceSyncRequest(
      const cryptauthv2::ClientMetadata& expected_client_metadata) {
    expected_client_metadata_list_.push_back(expected_client_metadata);

    VerifyDeviceManagerObserversNotifiedOfStart(expected_client_metadata_list_);
    VerifyInvocationReasonHistogram(expected_client_metadata_list_);
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

  void VerifyInvocationReasonHistogram(
      const std::vector<cryptauthv2::ClientMetadata>&
          expected_client_metadata_list) {
    histogram_tester_.ExpectTotalCount(
        "CryptAuth.DeviceSyncV2.InvocationReason",
        expected_client_metadata_list.size());

    std::unordered_map<cryptauthv2::ClientMetadata::InvocationReason, size_t>
        reason_to_count_map;
    for (const cryptauthv2::ClientMetadata& metadata :
         expected_client_metadata_list) {
      ++reason_to_count_map[metadata.invocation_reason()];
    }

    for (const auto& reason_count_pair : reason_to_count_map) {
      histogram_tester_.ExpectBucketCount(
          "CryptAuth.DeviceSyncV2.InvocationReason", reason_count_pair.first,
          reason_count_pair.second);
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
  // device manager's observers, the scheduler, and the metrics loggers.
  void ProcessAndVerifyNewDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) {
    expected_device_sync_results_.push_back(device_sync_result);

    EXPECT_EQ(expected_device_sync_results_,
              device_sync_results_sent_to_observer_);
    EXPECT_EQ(expected_device_sync_results_,
              fake_scheduler_.handled_device_sync_results());
    VerifyDeviceSyncResultHistograms(expected_device_sync_results_);
  }

  void VerifyDeviceSyncResultHistograms(
      const std::vector<CryptAuthDeviceSyncResult>
          expected_device_sync_results) {
    histogram_tester_.ExpectTotalCount(
        "CryptAuth.DeviceSyncV2.Result.ResultCode",
        expected_device_sync_results.size());
    histogram_tester_.ExpectTotalCount(
        "CryptAuth.DeviceSyncV2.Result.ResultType",
        expected_device_sync_results.size());
    histogram_tester_.ExpectTotalCount(
        "CryptAuth.DeviceSyncV2.Result.DidDeviceRegistryChange",
        expected_device_sync_results.size());

    std::unordered_map<CryptAuthDeviceSyncResult::ResultCode, size_t>
        result_code_to_count_map;
    std::unordered_map<CryptAuthDeviceSyncResult::ResultType, size_t>
        result_type_to_count_map;
    size_t device_registry_changed_count = 0;
    for (const auto& result : expected_device_sync_results) {
      ++result_code_to_count_map[result.result_code()];
      ++result_type_to_count_map[result.GetResultType()];
      if (result.did_device_registry_change())
        ++device_registry_changed_count;
    }

    for (const auto& result_count_pair : result_code_to_count_map) {
      histogram_tester_.ExpectBucketCount(
          "CryptAuth.DeviceSyncV2.Result.ResultCode", result_count_pair.first,
          result_count_pair.second);
    }

    for (const auto& type_count_pair : result_type_to_count_map) {
      histogram_tester_.ExpectBucketCount(
          "CryptAuth.DeviceSyncV2.Result.ResultType", type_count_pair.first,
          type_count_pair.second);
    }

    histogram_tester_.ExpectBucketCount(
        "CryptAuth.DeviceSyncV2.Result.DidDeviceRegistryChange", true,
        device_registry_changed_count);
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<cryptauthv2::ClientMetadata> expected_client_metadata_list_;
  std::vector<CryptAuthDeviceSyncResult> expected_device_sync_results_;

  std::vector<cryptauthv2::ClientMetadata> client_metadata_sent_to_observer_;
  std::vector<CryptAuthDeviceSyncResult> device_sync_results_sent_to_observer_;

  TestingPrefServiceSimple test_pref_service_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  FakeCryptAuthSchedulerUpdatedByDeviceSyncResults fake_scheduler_;
  MockCryptAuthClientFactory mock_client_factory_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<CryptAuthDeviceRegistry> device_registry_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<FakeCryptAuthDeviceSyncerFactory> fake_device_syncer_factory_;
  std::unique_ptr<FakeAttestationCertificatesSyncerFactory>
      fake_attestation_certificates_syncer_factory_;
  FakeSyncedBluetoothAddressTrackerFactory
      fake_synced_bluetooth_address_tracker_factory_;

  std::unique_ptr<CryptAuthV2DeviceManager> device_manager_;
};

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest,
       RequestDeviceSyncThroughScheduler) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::INITIALIZATION,
      std::nullopt /* session_id */);
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
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* did_device_registry_change */,
                                cryptauthv2::GetClientDirectiveForTest()));
}

TEST_F(DeviceSyncCryptAuthV2DeviceManagerImplTest, FailureRetry) {
  CreateAndStartDeviceManager();
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, std::nullopt /* session_id */);

  // Fail first attempt with fatal error.
  FinishDeviceSyncAttemptAndVerifyResult(
      0u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::
              kErrorSyncMetadataApiCallInternalServerError,
          false /* did_device_registry_change */,
          std::nullopt /* client_directive */));
  EXPECT_EQ(kFakeFailureRetryTime, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(std::nullopt, device_manager()->GetLastDeviceSyncTime());
  EXPECT_TRUE(device_manager()->IsRecoveringFromFailure());

  // Fail second attempt with non-fatal error.
  RequestDeviceSyncThroughSchedulerAndVerify(
      cryptauthv2::ClientMetadata::PERIODIC, kFakeSessionId);
  FinishDeviceSyncAttemptAndVerifyResult(
      1u /* expected_device_sync_instance_index */,
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          false /* did_device_registry_change */,
          std::nullopt /* client_directive */));
  EXPECT_EQ(kFakeFailureRetryTime, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(std::nullopt, device_manager()->GetLastDeviceSyncTime());
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
  EXPECT_EQ(std::nullopt, device_manager()->GetTimeToNextAttempt());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(kFakeLastSuccessTimeSeconds),
            device_manager()->GetLastDeviceSyncTime());
  EXPECT_FALSE(device_manager()->IsRecoveringFromFailure());
}

}  // namespace device_sync

}  // namespace ash
