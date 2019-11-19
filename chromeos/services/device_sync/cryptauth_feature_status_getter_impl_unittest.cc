// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_getter_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_device.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";

const cryptauthv2::ClientMetadata& GetClientMetadata() {
  static const base::NoDestructor<cryptauthv2::ClientMetadata> client_metadata(
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC));
  return *client_metadata;
}

const cryptauthv2::RequestContext& GetRequestContext() {
  static const base::NoDestructor<cryptauthv2::RequestContext> request_context(
      [] {
        return cryptauthv2::BuildRequestContext(
            CryptAuthKeyBundle::KeyBundleNameEnumToString(
                CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether),
            GetClientMetadata(),
            cryptauthv2::GetClientAppMetadataForTest().instance_id(),
            cryptauthv2::GetClientAppMetadataForTest().instance_id_token());
      }());

  return *request_context;
}

cryptauthv2::DeviceFeatureStatus ConvertDeviceToDeviceFeatureStatus(
    const CryptAuthDevice& device,
    const base::flat_set<CryptAuthFeatureType>& feature_types) {
  cryptauthv2::DeviceFeatureStatus device_feature_status;
  device_feature_status.set_device_id(device.instance_id());

  int64_t last_modified_time_offset_millis = 0;
  for (CryptAuthFeatureType feature_type : feature_types) {
    bool is_supported_feature_type =
        base::Contains(GetSupportedCryptAuthFeatureTypes(), feature_type);

    const auto it = device.feature_states.find(
        CryptAuthFeatureTypeToSoftwareFeature(feature_type));
    bool is_supported =
        it != device.feature_states.end() &&
        it->second != multidevice::SoftwareFeatureState::kNotSupported;
    bool is_enabled = it != device.feature_states.end() &&
                      it->second == multidevice::SoftwareFeatureState::kEnabled;

    cryptauthv2::DeviceFeatureStatus::FeatureStatus* feature_status =
        device_feature_status.add_feature_statuses();

    // The first feature type in the set will have the device.last_update_time
    // as the last_modified_time_millis. All other feature types will have
    // smaller last_modified_time_millis.
    feature_status->set_last_modified_time_millis(
        std::max(0L, device.last_update_time.ToJavaTime() -
                         last_modified_time_offset_millis));
    ++last_modified_time_offset_millis;

    feature_status->set_feature_type(
        CryptAuthFeatureTypeToString(feature_type));
    if (is_supported_feature_type) {
      feature_status->set_enabled(is_supported);
    } else {
      EXPECT_TRUE(
          base::Contains(GetEnabledCryptAuthFeatureTypes(), feature_type));
      feature_status->set_enabled(is_enabled);
    }
  }

  return device_feature_status;
}

}  // namespace

class DeviceSyncCryptAuthFeatureStatusGetterImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 protected:
  DeviceSyncCryptAuthFeatureStatusGetterImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)) {
    client_factory_->AddObserver(this);
  }

  ~DeviceSyncCryptAuthFeatureStatusGetterImplTest() override {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();

    feature_status_getter_ =
        CryptAuthFeatureStatusGetterImpl::Factory::Get()->BuildInstance(
            client_factory_.get(), std::move(mock_timer));
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client,
            BatchGetFeatureStatuses(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthFeatureStatusGetterImplTest::
                             OnBatchGetFeatureStatuses));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void GetFeatureStatuses(const base::flat_set<std::string>& device_ids) {
    feature_status_getter_->GetFeatureStatuses(
        GetRequestContext(), device_ids,
        base::BindOnce(&DeviceSyncCryptAuthFeatureStatusGetterImplTest::
                           OnGetFeatureStatusesComplete,
                       base::Unretained(this)));
  }

  void VerifyBatchGetFeatureStatusesRequest(
      const base::flat_set<std::string>& expected_device_ids) {
    ASSERT_TRUE(batch_get_feature_statuses_request_);
    EXPECT_TRUE(batch_get_feature_statuses_success_callback_);
    EXPECT_TRUE(batch_get_feature_statuses_failure_callback_);

    EXPECT_EQ(
        GetRequestContext().SerializeAsString(),
        batch_get_feature_statuses_request_->context().SerializeAsString());
    EXPECT_EQ(expected_device_ids,
              base::flat_set<std::string>(
                  batch_get_feature_statuses_request_->device_ids().begin(),
                  batch_get_feature_statuses_request_->device_ids().end()));
    EXPECT_EQ(GetAllCryptAuthFeatureTypeStrings(),
              base::flat_set<std::string>(
                  batch_get_feature_statuses_request_->feature_types().begin(),
                  batch_get_feature_statuses_request_->feature_types().end()));
  }

  void SendCorrectBatchGetFeatureStatusesResponse(
      const base::flat_set<std::string>& device_ids,
      const base::flat_set<CryptAuthFeatureType>& feature_types) {
    cryptauthv2::BatchGetFeatureStatusesResponse response;
    for (const std::string& device_id : device_ids) {
      base::Optional<CryptAuthDevice> device = GetTestDeviceWithId(device_id);
      if (!device)
        continue;

      response.add_device_feature_statuses()->CopyFrom(
          ConvertDeviceToDeviceFeatureStatus(*device, feature_types));
    }
    ASSERT_TRUE(batch_get_feature_statuses_success_callback_);
    std::move(batch_get_feature_statuses_success_callback_).Run(response);
  }

  void SendCustomBatchGetFeatureStatusesResponse(
      const cryptauthv2::BatchGetFeatureStatusesResponse& response) {
    ASSERT_TRUE(batch_get_feature_statuses_success_callback_);
    std::move(batch_get_feature_statuses_success_callback_).Run(response);
  }

  void FailBatchGetFeatureStatusesRequest(
      const NetworkRequestError& network_request_error) {
    ASSERT_TRUE(batch_get_feature_statuses_failure_callback_);
    std::move(batch_get_feature_statuses_failure_callback_)
        .Run(network_request_error);
  }

  void VerifyGetFeatureStatuesResult(
      const base::flat_set<std::string>& expected_device_ids,
      CryptAuthDeviceSyncResult::ResultCode expected_result_code) {
    ASSERT_TRUE(device_sync_result_code_);
    EXPECT_EQ(expected_device_ids.size(),
              id_to_device_software_feature_info_map_.size());
    EXPECT_EQ(expected_result_code, device_sync_result_code_);

    for (const std::string& id : expected_device_ids) {
      const auto it = id_to_device_software_feature_info_map_.find(id);
      ASSERT_TRUE(it != id_to_device_software_feature_info_map_.end());
      EXPECT_EQ(GetTestDeviceWithId(id).feature_states,
                it->second.feature_state_map);
      EXPECT_EQ(GetTestDeviceWithId(id).last_update_time,
                it->second.last_modified_time);
    }
  }

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  void OnBatchGetFeatureStatuses(
      const cryptauthv2::BatchGetFeatureStatusesRequest& request,
      const CryptAuthClient::BatchGetFeatureStatusesCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    EXPECT_FALSE(batch_get_feature_statuses_request_);
    EXPECT_FALSE(batch_get_feature_statuses_success_callback_);
    EXPECT_FALSE(batch_get_feature_statuses_failure_callback_);

    batch_get_feature_statuses_request_ = request;
    batch_get_feature_statuses_success_callback_ = callback;
    batch_get_feature_statuses_failure_callback_ = error_callback;
  }

  void OnGetFeatureStatusesComplete(
      const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
          id_to_device_software_feature_info_map,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    id_to_device_software_feature_info_map_ =
        id_to_device_software_feature_info_map;
    device_sync_result_code_ = device_sync_result_code;
  }

  base::Optional<cryptauthv2::BatchGetFeatureStatusesRequest>
      batch_get_feature_statuses_request_;
  CryptAuthClient::BatchGetFeatureStatusesCallback
      batch_get_feature_statuses_success_callback_;
  CryptAuthClient::ErrorCallback batch_get_feature_statuses_failure_callback_;

  CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap
      id_to_device_software_feature_info_map_;
  base::Optional<CryptAuthDeviceSyncResult::ResultCode>
      device_sync_result_code_;

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  base::MockOneShotTimer* timer_;

  std::unique_ptr<CryptAuthFeatureStatusGetter> feature_status_getter_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthFeatureStatusGetterImplTest);
};

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest, Success) {
  GetFeatureStatuses(GetAllTestDeviceIds());

  VerifyBatchGetFeatureStatusesRequest(GetAllTestDeviceIds());

  SendCorrectBatchGetFeatureStatusesResponse(GetAllTestDeviceIds(),
                                             GetAllCryptAuthFeatureTypes());

  VerifyGetFeatureStatuesResult(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       FinishedWithNonFatalErrors_UnknownFeatureType) {
  base::flat_set<std::string> device_ids = {
      GetLocalDeviceMetadataPacketForTest().device_id()};
  GetFeatureStatuses(device_ids);

  VerifyBatchGetFeatureStatusesRequest(device_ids);

  // Include an unknown feature type string in the response. The unknown feature
  // type should be ignored.
  cryptauthv2::DeviceFeatureStatus status = ConvertDeviceToDeviceFeatureStatus(
      GetLocalDeviceForTest(), GetAllCryptAuthFeatureTypes());
  status.add_feature_statuses()->set_feature_type("Unknown_feature_type");

  cryptauthv2::BatchGetFeatureStatusesResponse response;
  response.add_device_feature_statuses()->CopyFrom(status);
  SendCustomBatchGetFeatureStatusesResponse(response);

  VerifyGetFeatureStatuesResult(
      device_ids,
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       FinishedWithNonFatalErrors_UnsupportedFeatureMarkedEnabled) {
  base::flat_set<std::string> device_ids = {
      GetLocalDeviceMetadataPacketForTest().device_id()};
  GetFeatureStatuses(device_ids);

  VerifyBatchGetFeatureStatusesRequest(device_ids);

  cryptauthv2::DeviceFeatureStatus status = ConvertDeviceToDeviceFeatureStatus(
      GetLocalDeviceForTest(), GetAllCryptAuthFeatureTypes());

  // The BetterTogether host feature is not supported for the local device.
  EXPECT_EQ(multidevice::SoftwareFeatureState::kNotSupported,
            GetLocalDeviceForTest()
                .feature_states
                .find(multidevice::SoftwareFeature::kBetterTogetherHost)
                ->second);

  // Ensure that BetterTogether host is marked as not supported in the response.
  auto beto_host_supported_it = std::find_if(
      status.mutable_feature_statuses()->begin(),
      status.mutable_feature_statuses()->end(),
      [](const cryptauthv2::DeviceFeatureStatus::FeatureStatus&
             feature_status) {
        return feature_status.feature_type() ==
               CryptAuthFeatureTypeToString(
                   CryptAuthFeatureType::kBetterTogetherHostSupported);
      });
  EXPECT_FALSE(beto_host_supported_it->enabled());

  // Erroneously mark the BetterTogether host feature state as enabled in the
  // response though it is not supported.
  auto beto_host_enabled_it = std::find_if(
      status.mutable_feature_statuses()->begin(),
      status.mutable_feature_statuses()->end(),
      [](const cryptauthv2::DeviceFeatureStatus::FeatureStatus&
             feature_status) {
        return feature_status.feature_type() ==
               CryptAuthFeatureTypeToString(
                   CryptAuthFeatureType::kBetterTogetherHostEnabled);
      });
  beto_host_enabled_it->set_enabled(true);

  cryptauthv2::BatchGetFeatureStatusesResponse response;
  response.add_device_feature_statuses()->CopyFrom(status);
  SendCustomBatchGetFeatureStatusesResponse(response);

  // The final output BetterTogether host state should continue to be
  // unsupported for the local device.
  VerifyGetFeatureStatuesResult(
      device_ids,
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       FinishedWithNonFatalErrors_UnrequestedDevicesInResponse) {
  base::flat_set<std::string> requested_device_ids = {
      GetLocalDeviceMetadataPacketForTest().device_id()};
  GetFeatureStatuses(requested_device_ids);

  VerifyBatchGetFeatureStatusesRequest(requested_device_ids);

  // Include features statuses for unrequested devices. These extra devices
  // should be ignored.
  SendCorrectBatchGetFeatureStatusesResponse(GetAllTestDeviceIds(),
                                             GetAllCryptAuthFeatureTypes());

  VerifyGetFeatureStatuesResult(
      requested_device_ids,
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       FinishedWithNonFatalErrors_DuplicateDeviceIdsInResponse) {
  base::flat_set<std::string> requested_device_ids = {
      GetLocalDeviceMetadataPacketForTest().device_id()};
  GetFeatureStatuses(requested_device_ids);

  VerifyBatchGetFeatureStatusesRequest(requested_device_ids);

  // Send duplicate local device entries in the response. These duplicate
  // entries should be ignored.
  cryptauthv2::DeviceFeatureStatus status = ConvertDeviceToDeviceFeatureStatus(
      GetLocalDeviceForTest(), GetAllCryptAuthFeatureTypes());
  cryptauthv2::BatchGetFeatureStatusesResponse response;
  response.add_device_feature_statuses()->CopyFrom(status);
  response.add_device_feature_statuses()->CopyFrom(status);
  SendCustomBatchGetFeatureStatusesResponse(response);

  VerifyGetFeatureStatuesResult(
      requested_device_ids,
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       FinishedWithNonFatalErrors_DevicesMissingInResponse) {
  GetFeatureStatuses(GetAllTestDeviceIds());

  VerifyBatchGetFeatureStatusesRequest(GetAllTestDeviceIds());

  // Send feature statuses for only one of the three requested devices.
  base::flat_set<std::string> returned_device_ids = {
      GetLocalDeviceMetadataPacketForTest().device_id()};
  SendCorrectBatchGetFeatureStatusesResponse(returned_device_ids,
                                             GetAllCryptAuthFeatureTypes());

  VerifyGetFeatureStatuesResult(
      returned_device_ids,
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       Failure_Timeout_BatchGetFeatureStatusesResponse) {
  GetFeatureStatuses(GetAllTestDeviceIds());

  VerifyBatchGetFeatureStatusesRequest(GetAllTestDeviceIds());

  timer()->Fire();

  VerifyGetFeatureStatuesResult(
      {} /* expected_device_ids */,
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse);
}

TEST_F(DeviceSyncCryptAuthFeatureStatusGetterImplTest,
       Failure_ApiCall_BatchGetFeatureStatuses) {
  GetFeatureStatuses(GetAllTestDeviceIds());

  VerifyBatchGetFeatureStatusesRequest(GetAllTestDeviceIds());

  FailBatchGetFeatureStatusesRequest(NetworkRequestError::kBadRequest);

  VerifyGetFeatureStatuesResult(
      {} /* expected_device_ids */,
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallBadRequest);
}

}  // namespace device_sync

}  // namespace chromeos
