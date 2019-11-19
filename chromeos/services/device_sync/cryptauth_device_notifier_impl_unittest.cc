// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_notifier_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_device_notifier.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";

const cryptauthv2::ClientMetadata& GetClientMetadata() {
  static const base::NoDestructor<cryptauthv2::ClientMetadata> client_metadata(
      [] {
        cryptauthv2::ClientMetadata client_metadata;
        client_metadata.set_invocation_reason(
            cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);
        return client_metadata;
      }());
  return *client_metadata;
}

const cryptauthv2::RequestContext& GetRequestContext() {
  static const base::NoDestructor<cryptauthv2::RequestContext> request_context(
      cryptauthv2::BuildRequestContext(
          CryptAuthKeyBundle::KeyBundleNameEnumToString(
              CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether),
          GetClientMetadata(),
          cryptauthv2::GetClientAppMetadataForTest().instance_id(),
          cryptauthv2::GetClientAppMetadataForTest().instance_id_token()));
  return *request_context;
}

// Request with |device_ids|, Enrollment as the target service, and
// BetterTogether host (enabled) as the feature type.
cryptauthv2::BatchNotifyGroupDevicesRequest
NotifyEnrollmentBetterTogetherHostEnabledRequest(
    const base::flat_set<std::string>& device_ids) {
  cryptauthv2::BatchNotifyGroupDevicesRequest request;
  request.mutable_context()->CopyFrom(GetRequestContext());
  *request.mutable_notify_device_ids() = {device_ids.begin(), device_ids.end()};
  request.set_target_service(cryptauthv2::TargetService::ENROLLMENT);
  request.set_feature_type(CryptAuthFeatureTypeToString(
      CryptAuthFeatureType::kBetterTogetherHostEnabled));

  return request;
}

// Request with |device_ids|, DeviceSync as the target service, and
// MagicTether client (supported) as the feature type.
cryptauthv2::BatchNotifyGroupDevicesRequest
NotifyDeviceSyncMagicTetherSupportedRequest(
    const base::flat_set<std::string>& device_ids) {
  cryptauthv2::BatchNotifyGroupDevicesRequest request;
  request.mutable_context()->CopyFrom(GetRequestContext());
  *request.mutable_notify_device_ids() = {device_ids.begin(), device_ids.end()};
  request.set_target_service(cryptauthv2::TargetService::DEVICE_SYNC);
  request.set_feature_type(CryptAuthFeatureTypeToString(
      CryptAuthFeatureType::kMagicTetherClientSupported));

  return request;
}

}  // namespace

class DeviceSyncCryptAuthDeviceNotifierImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 protected:
  enum class RequestAction { kSucceed, kFail, kTimeout };

  DeviceSyncCryptAuthDeviceNotifierImplTest()
      : mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS),
        fake_gcm_manager_(cryptauthv2::kTestGcmRegistrationId) {
    mock_client_factory_.AddObserver(this);
  }

  ~DeviceSyncCryptAuthDeviceNotifierImplTest() override {
    mock_client_factory_.RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    device_notifier_ =
        CryptAuthDeviceNotifierImpl::Factory::Get()->BuildInstance(
            &fake_client_app_metadata_provider_, &mock_client_factory_,
            &fake_gcm_manager_, std::move(mock_timer));
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client,
            BatchNotifyGroupDevices(testing::_, testing::_, testing::_))
        .WillByDefault(Invoke(this, &DeviceSyncCryptAuthDeviceNotifierImplTest::
                                        OnBatchNotifyGroupDevices));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void NotifyDevices(const base::flat_set<std::string>& device_ids,
                     cryptauthv2::TargetService target_service,
                     CryptAuthFeatureType feature_type) {
    device_notifier_->NotifyDevices(
        device_ids, target_service, feature_type,
        base::BindOnce(
            &DeviceSyncCryptAuthDeviceNotifierImplTest::OnNotifyDevicesSuccess,
            base::Unretained(this)),
        base::BindOnce(
            &DeviceSyncCryptAuthDeviceNotifierImplTest::OnNotifyDevicesFailure,
            base::Unretained(this)));
  }

  void HandleClientAppMetadataRequest(RequestAction request_action) {
    ASSERT_FALSE(
        fake_client_app_metadata_provider_.metadata_requests().empty());
    EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
              fake_client_app_metadata_provider_.metadata_requests()
                  .back()
                  .gcm_registration_id);
    switch (request_action) {
      case RequestAction::kSucceed:
        std::move(fake_client_app_metadata_provider_.metadata_requests()
                      .back()
                      .callback)
            .Run(cryptauthv2::GetClientAppMetadataForTest());
        return;
      case RequestAction::kFail:
        std::move(fake_client_app_metadata_provider_.metadata_requests()
                      .back()
                      .callback)
            .Run(base::nullopt /* client_app_metadata */);
        return;
      case RequestAction::kTimeout:
        mock_timer_->Fire();
        return;
    }
  }

  void HandleNextBatchNotifyGroupDevicesRequest(
      const cryptauthv2::BatchNotifyGroupDevicesRequest& expected_request,
      RequestAction request_action,
      base::Optional<NetworkRequestError> error = base::nullopt) {
    ASSERT_FALSE(batch_notify_group_devices_requests_.empty());

    cryptauthv2::BatchNotifyGroupDevicesRequest current_request =
        std::move(batch_notify_group_devices_requests_.front());
    batch_notify_group_devices_requests_.pop();

    CryptAuthClient::BatchNotifyGroupDevicesCallback current_success_callback =
        std::move(batch_notify_group_devices_success_callbacks_.front());
    batch_notify_group_devices_success_callbacks_.pop();

    CryptAuthClient::ErrorCallback current_failure_callback =
        std::move(batch_notify_group_devices_failure_callbacks_.front());
    batch_notify_group_devices_failure_callbacks_.pop();

    EXPECT_EQ(expected_request.SerializeAsString(),
              current_request.SerializeAsString());

    switch (request_action) {
      case RequestAction::kSucceed:
        std::move(current_success_callback)
            .Run(cryptauthv2::BatchNotifyGroupDevicesResponse());
        break;
      case RequestAction::kFail:
        ASSERT_TRUE(error);
        std::move(current_failure_callback).Run(*error);
        break;
      case RequestAction::kTimeout:
        mock_timer_->Fire();
        break;
    }
  }

  void VerifyNumberOfClientAppMetadataFetchAttempts(size_t num_attempts) {
    EXPECT_EQ(num_attempts,
              fake_client_app_metadata_provider_.metadata_requests().size());
  }

  void VerifyResults(
      const std::vector<base::Optional<NetworkRequestError>> expected_results) {
    // Verify that all requests were processed.
    EXPECT_TRUE(batch_notify_group_devices_requests_.empty());
    EXPECT_TRUE(batch_notify_group_devices_success_callbacks_.empty());
    EXPECT_TRUE(batch_notify_group_devices_failure_callbacks_.empty());

    EXPECT_EQ(expected_results, results_);
  }

 private:
  void OnBatchNotifyGroupDevices(
      const cryptauthv2::BatchNotifyGroupDevicesRequest& request,
      const CryptAuthClient::BatchNotifyGroupDevicesCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    batch_notify_group_devices_requests_.push(request);
    batch_notify_group_devices_success_callbacks_.push(std::move(callback));
    batch_notify_group_devices_failure_callbacks_.push(
        std::move(error_callback));
  }

  void OnNotifyDevicesSuccess() { results_.push_back(base::nullopt); }

  void OnNotifyDevicesFailure(NetworkRequestError error) {
    results_.push_back(error);
  }

  base::queue<cryptauthv2::BatchNotifyGroupDevicesRequest>
      batch_notify_group_devices_requests_;
  base::queue<CryptAuthClient::BatchNotifyGroupDevicesCallback>
      batch_notify_group_devices_success_callbacks_;
  base::queue<CryptAuthClient::ErrorCallback>
      batch_notify_group_devices_failure_callbacks_;

  // base::nullopt indicates a success.
  std::vector<base::Optional<NetworkRequestError>> results_;

  FakeClientAppMetadataProvider fake_client_app_metadata_provider_;
  MockCryptAuthClientFactory mock_client_factory_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  std::unique_ptr<CryptAuthDeviceNotifier> device_notifier_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthDeviceNotifierImplTest);
};

TEST_F(DeviceSyncCryptAuthDeviceNotifierImplTest, Test) {
  // Queue up 6 requests before any finish. They should be processed
  // sequentially.
  NotifyDevices({"device_id_1"}, cryptauthv2::TargetService::ENROLLMENT,
                CryptAuthFeatureType::kEasyUnlockClientEnabled);
  NotifyDevices({"device_id_2"}, cryptauthv2::TargetService::DEVICE_SYNC,
                CryptAuthFeatureType::kSmsConnectHostSupported);
  NotifyDevices({"device_id_3"}, cryptauthv2::TargetService::ENROLLMENT,
                CryptAuthFeatureType::kBetterTogetherHostEnabled);
  NotifyDevices({"device_id_4", "device_id_5"},
                cryptauthv2::TargetService::DEVICE_SYNC,
                CryptAuthFeatureType::kMagicTetherClientSupported);
  NotifyDevices({"device_id_6", "device_id_7"},
                cryptauthv2::TargetService::ENROLLMENT,
                CryptAuthFeatureType::kBetterTogetherHostEnabled);
  NotifyDevices({"device_id_8"}, cryptauthv2::TargetService::DEVICE_SYNC,
                CryptAuthFeatureType::kMagicTetherClientSupported);

  // base::nullopt indicates a success.
  std::vector<base::Optional<NetworkRequestError>> expected_results;

  // Timeout waiting for ClientAppMetadata.
  HandleClientAppMetadataRequest(RequestAction::kTimeout);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Fail ClientAppMetadata fetch.
  HandleClientAppMetadataRequest(RequestAction::kFail);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Timeout waiting for BatchNotifyGroupDevices.
  HandleClientAppMetadataRequest(RequestAction::kSucceed);
  HandleNextBatchNotifyGroupDevicesRequest(
      NotifyEnrollmentBetterTogetherHostEnabledRequest({"device_id_3"}),
      RequestAction::kTimeout);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Fail BatchNotifyGroupDevices call with "Bad Request".
  HandleNextBatchNotifyGroupDevicesRequest(
      NotifyDeviceSyncMagicTetherSupportedRequest(
          {"device_id_4", "device_id_5"}),
      RequestAction::kFail, NetworkRequestError::kBadRequest);
  expected_results.push_back(NetworkRequestError::kBadRequest);

  // Succeed notifying devices.
  HandleNextBatchNotifyGroupDevicesRequest(
      NotifyEnrollmentBetterTogetherHostEnabledRequest(
          {"device_id_6", "device_id_7"}),
      RequestAction::kSucceed);
  expected_results.push_back(base::nullopt);

  // Succeed notifying devices.
  HandleNextBatchNotifyGroupDevicesRequest(
      NotifyDeviceSyncMagicTetherSupportedRequest({"device_id_8"}),
      RequestAction::kSucceed);
  expected_results.push_back(base::nullopt);

  // There was 1 timeout, 1 failed attempt, and 1 successful attempt to retrieve
  // ClientAppMetadata.
  VerifyNumberOfClientAppMetadataFetchAttempts(3u);

  VerifyResults(expected_results);
}

}  // namespace device_sync

}  // namespace chromeos
