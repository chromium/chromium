// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_status_setter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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
          GetClientMetadata(), cryptauthv2::kTestInstanceId,
          cryptauthv2::kTestInstanceIdToken));
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
 public:
  DeviceSyncCryptAuthFeatureStatusSetterImplTest(
      const DeviceSyncCryptAuthFeatureStatusSetterImplTest&) = delete;
  DeviceSyncCryptAuthFeatureStatusSetterImplTest& operator=(
      const DeviceSyncCryptAuthFeatureStatusSetterImplTest&) = delete;

 protected:
  enum class RequestAction { kSucceed, kFail, kTimeout };

  DeviceSyncCryptAuthFeatureStatusSetterImplTest()
      : mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS) {
    mock_client_factory_.AddObserver(this);
  }

  ~DeviceSyncCryptAuthFeatureStatusSetterImplTest() override {
    mock_client_factory_.RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    feature_status_setter_ = CryptAuthFeatureStatusSetterImpl::Factory::Create(
        cryptauthv2::kTestInstanceId, cryptauthv2::kTestInstanceIdToken,
        &mock_client_factory_, std::move(mock_timer));
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

  void HandleNextBatchSetFeatureStatusesRequest(
      const cryptauthv2::BatchSetFeatureStatusesRequest& expected_request,
      RequestAction request_action,
      std::optional<NetworkRequestError> error = std::nullopt) {
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

  void VerifyResults(
      const std::vector<std::optional<NetworkRequestError>> expected_results) {
    // Verify that all requests were processed.
    EXPECT_TRUE(batch_set_feature_statuses_requests_.empty());
    EXPECT_TRUE(batch_set_feature_statuses_success_callbacks_.empty());
    EXPECT_TRUE(batch_set_feature_statuses_failure_callbacks_.empty());

    EXPECT_EQ(expected_results, results_);
  }

 private:
  void OnBatchSetFeatureStatuses(
      const cryptauthv2::BatchSetFeatureStatusesRequest& request,
      CryptAuthClient::BatchSetFeatureStatusesCallback callback,
      CryptAuthClient::ErrorCallback error_callback) {
    batch_set_feature_statuses_requests_.push(request);
    batch_set_feature_statuses_success_callbacks_.push(std::move(callback));
    batch_set_feature_statuses_failure_callbacks_.push(
        std::move(error_callback));
  }

  void OnSetFeatureStatusSuccess() { results_.push_back(std::nullopt); }

  void OnSetFeatureStatusFailure(NetworkRequestError error) {
    results_.push_back(error);
  }

  base::queue<cryptauthv2::BatchSetFeatureStatusesRequest>
      batch_set_feature_statuses_requests_;
  base::queue<CryptAuthClient::BatchSetFeatureStatusesCallback>
      batch_set_feature_statuses_success_callbacks_;
  base::queue<CryptAuthClient::ErrorCallback>
      batch_set_feature_statuses_failure_callbacks_;

  // std::nullopt indicates a success.
  std::vector<std::optional<NetworkRequestError>> results_;

  MockCryptAuthClientFactory mock_client_factory_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_ = nullptr;

  std::unique_ptr<CryptAuthFeatureStatusSetter> feature_status_setter_;
};

TEST_F(DeviceSyncCryptAuthFeatureStatusSetterImplTest, Test) {
  // Queue up 4 requests before any finish. They should be processed
  // sequentially.
  SetFeatureStatus("device_id_1", multidevice::SoftwareFeature::kSmartLockHost,
                   FeatureStatusChange::kEnableExclusively);
  SetFeatureStatus("device_id_2",
                   multidevice::SoftwareFeature::kInstantTetheringClient,
                   FeatureStatusChange::kDisable);
  SetFeatureStatus("device_id_3",
                   multidevice::SoftwareFeature::kInstantTetheringClient,
                   FeatureStatusChange::kDisable);
  SetFeatureStatus("device_id_4",
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableNonExclusively);

  // std::nullopt indicates a success.
  std::vector<std::optional<NetworkRequestError>> expected_results;

  // Timeout waiting for BatchSetFeatureStatuses.
  HandleNextBatchSetFeatureStatusesRequest(
      SmartLockHostExclusivelyEnabledRequest("device_id_1"),
      RequestAction::kTimeout);
  expected_results.push_back(NetworkRequestError::kUnknown);

  // Fail BatchSetFeatureStatuses call with "Bad Request".
  HandleNextBatchSetFeatureStatusesRequest(
      InstantTetherClientDisabledRequest("device_id_2"), RequestAction::kFail,
      NetworkRequestError::kBadRequest);
  expected_results.push_back(NetworkRequestError::kBadRequest);

  // Succeed disabling InstantTethering client.
  HandleNextBatchSetFeatureStatusesRequest(
      InstantTetherClientDisabledRequest("device_id_3"),
      RequestAction::kSucceed);
  expected_results.push_back(std::nullopt);

  // Succeed enabling BetterTogether host.
  HandleNextBatchSetFeatureStatusesRequest(
      BetterTogetherHostEnabledRequest("device_id_4"), RequestAction::kSucceed);
  expected_results.push_back(std::nullopt);

  VerifyResults(expected_results);
}

}  // namespace device_sync

}  // namespace ash
