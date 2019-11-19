// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_setter_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/feature_status_change.h"
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
            cryptauthv2::ClientMetadata::FEATURE_TOGGLED);
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

cryptauthv2::BatchSetFeatureStatusesRequest BetterTogetherHostEnabledRequest(
    const std::string& device_id) {
  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(GetRequestContext());

  cryptauthv2::DeviceFeatureStatus* device_feature_status =
      request.add_device_feature_statuses();
  device_feature_status->set_device_id(device_id);

  cryptauthv2::DeviceFeatureStatus::FeatureStatus* feature_status =
      device_feature_status->add_feature_statuses();
  feature_status->set_feature_type(CryptAuthFeatureTypeToString(
      CryptAuthFeatureType::kBetterTogetherHostEnabled));
  feature_status->set_enabled(true);
  feature_status->set_enable_exclusively(false);

  return request;
}

cryptauthv2::BatchSetFeatureStatusesRequest
SmartLockHostExclusivelyEnabledRequest(const std::string& device_id) {
  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(GetRequestContext());

  cryptauthv2::DeviceFeatureStatus* device_feature_status =
      request.add_device_feature_statuses();
  device_feature_status->set_device_id(device_id);

  cryptauthv2::DeviceFeatureStatus::FeatureStatus* feature_status =
      device_feature_status->add_feature_statuses();
  feature_status->set_feature_type(CryptAuthFeatureTypeToString(
      CryptAuthFeatureType::kEasyUnlockHostEnabled));
  feature_status->set_enabled(true);
  feature_status->set_enable_exclusively(true);
  return request;
}

cryptauthv2::BatchSetFeatureStatusesRequest InstantTetherClientDisabledRequest(
    const std::string& device_id) {
  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(GetRequestContext());

  cryptauthv2::DeviceFeatureStatus* device_feature_status =
      request.add_device_feature_statuses();
  device_feature_status->set_device_id(device_id);

  cryptauthv2::DeviceFeatureStatus::FeatureStatus* feature_status =
      device_feature_status->add_feature_statuses();
  feature_status->set_feature_type(CryptAuthFeatureTypeToString(
      CryptAuthFeatureType::kMagicTetherClientEnabled));
  feature_status->set_enabled(false);
  feature_status->set_enable_exclusively(false);

  return request;
}

}  // namespace

class DeviceSyncCryptAuthFeatureStatusSetterImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 protected:
  enum class RequestAction { kSucceed, kFail, kTimeout };

  DeviceSyncCryptAuthFeatureStatusSetterImplTest()
      : mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS),
        fake_gcm_manager_(cryptauthv2::kTestGcmRegistrationId) {
    mock_client_factory_.AddObserver(this);
  }

  ~DeviceSyncCryptAuthFeatureStatusSetterImplTest() override {
    mock_client_factory_.RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    feature_status_setter_ =
        CryptAuthFeatureStatusSetterImpl::Factory::Get()->BuildInstance(
            &fake_client_app_metadata_provider_, &mock_client_factory_,
            &fake_gcm_manager_, std::move(mock_timer));
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client,
            BatchSetFeatureStatuses(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthFeatureStatusSetterImplTest::
                             OnBatchSetFeatureStatuses));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void SetFeatureStatus(const std::string& device_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change) {
    feature_status_setter_->SetFeatureStatus(
        device_id, feature, status_change,
        base::BindOnce(&DeviceSyncCryptAuthFeatureStatusSetterImplTest::
                           OnSetFeatureStatusSuccess,
                       base::Unretained(this)),
        base::BindOnce(&DeviceSyncCryptAuthFeatureStatusSetterImplTest::
                           OnSetFeatureStatusFailure,
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

  void HandleNextBatchSetFeatureStatusesRequest(
      const cryptauthv2::BatchSetFeatureStatusesRequest& expected_request,
      RequestAction request_action,
      base::Optional<NetworkRequestError> error = base::nullopt) {
    ASSERT_TRUE(!batch_set_feature_statuses_requests_.empty());

    cryptauthv2::BatchSetFeatureStatusesRequest current_request =
        std::move(batch_set_feature_statuses_requests_.front());
    batch_set_feature_statuses_requests_.pop();

    CryptAuthClient::BatchSetFeatureStatusesCallback current_success_callback =
        std::move(batch_set_feature_statuses_success_callbacks_.front());
    batch_set_feature_statuses_success_callbacks_.pop();

    CryptAuthClient::ErrorCallback current_failure_callback =
        std::move(batch_set_feature_statuses_failure_callbacks_.front());
    batch_set_feature_statuses_failure_callbacks_.pop();

    EXPECT_EQ(expected_request.SerializeAsString(),
              current_request.SerializeAsString());

    switch (request_action) {
      case RequestAction::kSucceed:
        std::move(current_success_callback)
            .Run(cryptauthv2::BatchSetFeatureStatusesResponse());
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
    EXPECT_TRUE(batch_set_feature_statuses_requests_.empty());
    EXPECT_TRUE(batch_set_feature_statuses_success_callbacks_.empty());
    EXPECT_TRUE(batch_set_feature_statuses_failure_callbacks_.empty());

    EXPECT_EQ(expected_results, results_);
  }

 private:
  void OnBatchSetFeatureStatuses(
      const cryptauthv2::BatchSetFeatureStatusesRequest& request,
      const CryptAuthClient::BatchSetFeatureStatusesCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    batch_set_feature_statuses_requests_.push(request);
    batch_set_feature_statuses_success_callbacks_.push(std::move(callback));
    batch_set_feature_statuses_failure_callbacks_.push(
        std::move(error_callback));
  }

  void OnSetFeatureStatusSuccess() { results_.push_back(base::nullopt); }

  void OnSetFeatureStatusFailure(NetworkRequestError error) {
    results_.push_back(error);
  }

  base::queue<cryptauthv2::BatchSetFeatureStatusesRequest>
      batch_set_feature_statuses_requests_;
  base::queue<CryptAuthClient::BatchSetFeatureStatusesCallback>
      batch_set_feature_statuses_success_callbacks_;
  base::queue<CryptAuthClient::ErrorCallback>
      batch_set_feature_statuses_failure_callbacks_;

  // base::nullopt indicates a success.
  std::vector<base::Optional<NetworkRequestError>> results_;

  FakeClientAppMetadataProvider fake_client_app_metadata_provider_;
  MockCryptAuthClientFactory mock_client_factory_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  std::unique_ptr<CryptAuthFeatureStatusSetter> feature_status_setter_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthFeatureStatusSetterImplTest);
};

TEST_F(DeviceSyncCryptAuthFeatureStatusSetterImplTest, Test) {
  // Queue up 6 requests before any finish. They should be processed
  // sequentially.
  SetFeatureStatus("device_id_1",
                   multidevice::SoftwareFeature::kSmartLockClient,
                   FeatureStatusChange::kDisable);
  SetFeatureStatus("device_id_2",
                   multidevice::SoftwareFeature::kInstantTetheringHost,
                   FeatureStatusChange::kEnableNonExclusively);
  SetFeatureStatus("device_id_3", multidevice::SoftwareFeature::kSmartLockHost,
                   FeatureStatusChange::kEnableExclusively);
  SetFeatureStatus("device_id_4",
                   multidevice::SoftwareFeature::kInstantTetheringClient,
                   FeatureStatusChange::kDisable);
  SetFeatureStatus("device_id_5",
                   multidevice::SoftwareFeature::kInstantTetheringClient,
                   FeatureStatusChange::kDisable);
  SetFeatureStatus("device_id_6",
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableNonExclusively);

  // base::nullopt indicates a success.
  std::vector<base::Optional<NetworkRequestError>> expected_results;

  // Timeout waiting for ClientAppMetadata.
  HandleClientAppMetadataRequest(RequestAction::kTimeout);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Fail ClientAppMetadata fetch.
  HandleClientAppMetadataRequest(RequestAction::kFail);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Timeout waiting for BatchSetFeatureStatuses.
  HandleClientAppMetadataRequest(RequestAction::kSucceed);
  HandleNextBatchSetFeatureStatusesRequest(
      SmartLockHostExclusivelyEnabledRequest("device_id_3"),
      RequestAction::kTimeout);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Fail BatchSetFeatureStatuses call with "Bad Request".
  HandleNextBatchSetFeatureStatusesRequest(
      InstantTetherClientDisabledRequest("device_id_4"), RequestAction::kFail,
      NetworkRequestError::kBadRequest);
  expected_results.push_back(NetworkRequestError::kBadRequest);

  // Succeed disabling InstantTethering client.
  HandleNextBatchSetFeatureStatusesRequest(
      InstantTetherClientDisabledRequest("device_id_5"),
      RequestAction::kSucceed);
  expected_results.push_back(base::nullopt);

  // Succeed enabling BetterTogether host.
  HandleNextBatchSetFeatureStatusesRequest(
      BetterTogetherHostEnabledRequest("device_id_6"), RequestAction::kSucceed);
  expected_results.push_back(base::nullopt);

  // There was 1 timeout, 1 failed attempt, and 1 successful attempt.
  VerifyNumberOfClientAppMetadataFetchAttempts(3u);

  VerifyResults(expected_results);
}

}  // namespace device_sync

}  // namespace chromeos
