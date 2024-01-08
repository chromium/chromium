// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_activity_getter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/ash/services/device_sync/device_sync_type_converters.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";
const char kDeviceId[] = "device_id1";
const int kLastActivityTimeSecs = 111;
const int kLastUpdateTimeSecs = 222;
const cryptauthv2::ConnectivityStatus kConnectivityStatus =
    cryptauthv2::ConnectivityStatus::ONLINE;

const cryptauthv2::ClientMetadata& GetClientMetadata() {
  static const base::NoDestructor<cryptauthv2::ClientMetadata> client_metadata(
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED));
  return *client_metadata;
}

const cryptauthv2::RequestContext& GetRequestContext() {
  static const base::NoDestructor<cryptauthv2::RequestContext> request_context(
      [] {
        return cryptauthv2::BuildRequestContext(
            CryptAuthKeyBundle::KeyBundleNameEnumToString(
                CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether),
            GetClientMetadata(), cryptauthv2::kTestInstanceId,
            cryptauthv2::kTestInstanceIdToken);
      }());
  return *request_context;
}

const cryptauthv2::GetDevicesActivityStatusResponse& GetResponse() {
  static const base::NoDestructor<cryptauthv2::GetDevicesActivityStatusResponse>
      activity_response([] {
        cryptauthv2::GetDevicesActivityStatusResponse response;
        cryptauthv2::Timestamp last_update_time;
        last_update_time.set_seconds(kLastUpdateTimeSecs);
        response.add_device_activity_statuses()->CopyFrom(
            cryptauthv2::BuildDeviceActivityStatus(
                kDeviceId, kLastActivityTimeSecs, kConnectivityStatus,
                last_update_time));
        return response;
      }());

  return *activity_response;
}

}  // namespace

class DeviceSyncCryptAuthDeviceActivityGetterImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceSyncCryptAuthDeviceActivityGetterImplTest(
      const DeviceSyncCryptAuthDeviceActivityGetterImplTest&) = delete;
  DeviceSyncCryptAuthDeviceActivityGetterImplTest& operator=(
      const DeviceSyncCryptAuthDeviceActivityGetterImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthDeviceActivityGetterImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)) {
    client_factory_->AddObserver(this);
  }

  ~DeviceSyncCryptAuthDeviceActivityGetterImplTest() override {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();
    device_activity_getter_ =
        CryptAuthDeviceActivityGetterImpl::Factory::Create(
            cryptauthv2::kTestInstanceId, cryptauthv2::kTestInstanceIdToken,
            client_factory_.get(), std::move(mock_timer));
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client,
            GetDevicesActivityStatus(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthDeviceActivityGetterImplTest::
                             OnGetDevicesActivityStatus));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void GetDeviceActivityStatus() {
    device_activity_getter_->GetDevicesActivityStatus(
        base::BindOnce(&DeviceSyncCryptAuthDeviceActivityGetterImplTest::
                           OnGetDevicesActivityStatusFinished,
                       base::Unretained(this)),
        base::BindOnce(&DeviceSyncCryptAuthDeviceActivityGetterImplTest::
                           OnGetDevicesActivityStatusError,
                       base::Unretained(this)));
  }

  void VerifyGetDevicesActivityStatusRequest() {
    ASSERT_TRUE(get_device_activity_status_request_);
    EXPECT_TRUE(get_device_activity_status_success_callback_);
    EXPECT_TRUE(get_device_activity_status_failure_callback_);

    EXPECT_EQ(
        GetRequestContext().SerializeAsString(),
        get_device_activity_status_request_->context().SerializeAsString());
  }

  void SendGetDevicesActivityStatusResponse() {
    ASSERT_TRUE(get_device_activity_status_success_callback_);
    std::move(get_device_activity_status_success_callback_).Run(GetResponse());
  }

  void FailGetDevicesActivityStatusRequest(
      const NetworkRequestError& network_request_error) {
    ASSERT_TRUE(get_device_activity_status_failure_callback_);
    std::move(get_device_activity_status_failure_callback_)
        .Run(network_request_error);
  }

  void VerifyNetworkRequestResult(
      mojom::NetworkRequestResult expected_network_result) {
    EXPECT_EQ(expected_network_result, network_request_result_);
  }

  void VerifyDevicesActivityStatusResult(
      const std::vector<mojom::DeviceActivityStatusPtr>&
          expected_device_activity_status_result) {
    EXPECT_EQ(expected_device_activity_status_result,
              device_activity_status_result_);
  }

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  void OnGetDevicesActivityStatus(
      const cryptauthv2::GetDevicesActivityStatusRequest& request,
      CryptAuthClient::GetDevicesActivityStatusCallback callback,
      CryptAuthClient::ErrorCallback error_callback) {
    EXPECT_FALSE(get_device_activity_status_request_);
    EXPECT_FALSE(get_device_activity_status_success_callback_);
    EXPECT_FALSE(get_device_activity_status_failure_callback_);

    get_device_activity_status_request_ = request;
    get_device_activity_status_success_callback_ = std::move(callback);
    get_device_activity_status_failure_callback_ = std::move(error_callback);
  }

  void OnGetDevicesActivityStatusFinished(
      std::vector<mojom::DeviceActivityStatusPtr>
          device_activity_status_result) {
    device_activity_status_result_ = std::move(device_activity_status_result);
    network_request_result_ = mojom::NetworkRequestResult::kSuccess;
  }

  void OnGetDevicesActivityStatusError(NetworkRequestError error) {
    network_request_result_ =
        mojo::ConvertTo<mojom::NetworkRequestResult>(error);
  }

  std::optional<cryptauthv2::GetDevicesActivityStatusRequest>
      get_device_activity_status_request_;
  CryptAuthClient::GetDevicesActivityStatusCallback
      get_device_activity_status_success_callback_;
  CryptAuthClient::ErrorCallback get_device_activity_status_failure_callback_;

  std::vector<mojom::DeviceActivityStatusPtr> device_activity_status_result_;
  mojom::NetworkRequestResult network_request_result_;

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  std::unique_ptr<CryptAuthDeviceActivityGetter> device_activity_getter_;
};

TEST_F(DeviceSyncCryptAuthDeviceActivityGetterImplTest, Success) {
  GetDeviceActivityStatus();
  VerifyGetDevicesActivityStatusRequest();
  SendGetDevicesActivityStatusResponse();
  VerifyNetworkRequestResult(mojom::NetworkRequestResult::kSuccess);
  mojom::DeviceActivityStatusPtr expected_device_activity_status =
      mojom::DeviceActivityStatus::New(
          kDeviceId, base::Time::FromTimeT(kLastActivityTimeSecs),
          kConnectivityStatus, base::Time::FromTimeT(kLastUpdateTimeSecs));
  std::vector<mojom::DeviceActivityStatusPtr>
      expected_device_activity_status_result;
  expected_device_activity_status_result.emplace_back(
      std::move(expected_device_activity_status));
  VerifyDevicesActivityStatusResult(expected_device_activity_status_result);
}

TEST_F(DeviceSyncCryptAuthDeviceActivityGetterImplTest,
       Failure_Timeout_GetDevicesActivityStatusResponse) {
  GetDeviceActivityStatus();
  VerifyGetDevicesActivityStatusRequest();
  timer()->Fire();
  VerifyNetworkRequestResult(mojom::NetworkRequestResult::kUnknown);
}

TEST_F(DeviceSyncCryptAuthDeviceActivityGetterImplTest,
       Failure_ApiCall_GetDevicesActivityStatus) {
  GetDeviceActivityStatus();
  VerifyGetDevicesActivityStatusRequest();
  FailGetDevicesActivityStatusRequest(NetworkRequestError::kBadRequest);
  VerifyNetworkRequestResult(mojom::NetworkRequestResult::kBadRequest);
}

}  // namespace device_sync

}  // namespace ash
