// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_client_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/services/device_sync/cryptauth_api_call_flow.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_enrollment.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/proto/enum_util.h"
#include "chromeos/services/device_sync/switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace chromeos {

namespace device_sync {

namespace {

const char kTestGoogleApisUrl[] = "https://www.testgoogleapis.com";
const char kTestCryptAuthV2EnrollmentUrl[] =
    "https://cryptauthenrollment.testgoogleapis.com";
const char kTestCryptAuthV2DeviceSyncUrl[] =
    "https://cryptauthdevicesync.testgoogleapis.com";
const char kAccessToken[] = "access_token";
const char kEmail[] = "test@gmail.com";
const char kPublicKey1[] = "public_key1";
const char kPublicKey2[] = "public_key2";
const char kBluetoothAddress1[] = "AA:AA:AA:AA:AA:AA";
const char kBluetoothAddress2[] = "BB:BB:BB:BB:BB:BB";
const char kDeviceId1[] = "device_id1";
const char kDeviceId2[] = "device_id2";
const char kFeatureType1[] = "feature_type1";
const char kFeatureType2[] = "feature_type2";
const char kClientMetadataSessionId[] = "session_id";
const int kLastActivityTimeSecs1 = 111;
const int kLastActivityTimeSecs2 = 222;
const cryptauthv2::ConnectivityStatus kConnectivityStatus1 =
    cryptauthv2::ConnectivityStatus::ONLINE;
const cryptauthv2::ConnectivityStatus kConnectivityStatus2 =
    cryptauthv2::ConnectivityStatus::OFFLINE;

// Values for the DeviceClassifier field.
const int kDeviceOsVersionCode = 100;
const int kDeviceSoftwareVersionCode = 200;
const char kDeviceSoftwarePackage[] = "cryptauth_client_unittest";
const cryptauth::DeviceType kDeviceType = cryptauth::CHROME;

// Mock CryptAuthApiCallFlow, which handles the HTTP requests to CryptAuth.
class MockCryptAuthApiCallFlow : public CryptAuthApiCallFlow {
 public:
  MockCryptAuthApiCallFlow() : CryptAuthApiCallFlow() {
    SetPartialNetworkTrafficAnnotation(PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  }
  virtual ~MockCryptAuthApiCallFlow() {}

  MOCK_METHOD6(
      StartPostRequest,
      void(const GURL& request_url,
           const std::string& serialized_request,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           const std::string& access_token,
           const ResultCallback& result_callback,
           const ErrorCallback& error_callback));

  MOCK_METHOD6(
      StartGetRequest,
      void(const GURL& request_url,
           const std::vector<std::pair<std::string, std::string>>&
               request_as_query_parameters,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           const std::string& access_token,
           const ResultCallback& result_callback,
           const ErrorCallback& error_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthApiCallFlow);
};

// Callback that should never be invoked.
template <class T>
void NotCalled(T type) {
  EXPECT_TRUE(false);
}

// Callback that should never be invoked.
template <class T>
void NotCalledConstRef(const T& type) {
  EXPECT_TRUE(false);
}

// Callback that saves the result returned by CryptAuthClient.
template <class T>
void SaveResult(T* out, T result) {
  *out = result;
}

// Callback that saves the result returned by CryptAuthClient.
template <class T>
void SaveResultConstRef(T* out, const T& result) {
  *out = result;
}

}  // namespace

class DeviceSyncCryptAuthClientTest : public testing::Test {
 protected:
  DeviceSyncCryptAuthClientTest()
      : api_call_flow_(new StrictMock<MockCryptAuthApiCallFlow>()),
        serialized_request_(std::string()) {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kCryptAuthHTTPHost, kTestGoogleApisUrl);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kCryptAuthV2EnrollmentHTTPHost,
        kTestCryptAuthV2EnrollmentUrl);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kCryptAuthV2DeviceSyncHTTPHost,
        kTestCryptAuthV2DeviceSyncUrl);

    cryptauth::DeviceClassifier device_classifier;
    device_classifier.set_device_os_version_code(kDeviceOsVersionCode);
    device_classifier.set_device_software_version_code(
        kDeviceSoftwareVersionCode);
    device_classifier.set_device_software_package(kDeviceSoftwarePackage);
    device_classifier.set_device_type(DeviceTypeEnumToString(kDeviceType));

    identity_test_environment_.MakePrimaryAccountAvailable(kEmail);

    client_.reset(
        new CryptAuthClientImpl(base::WrapUnique(api_call_flow_),
                                identity_test_environment_.identity_manager(),
                                shared_factory_, device_classifier));
  }

  // Sets up an expectation and captures a CryptAuth API POST request to
  // |request_url|.
  void ExpectPostRequest(const std::string& request_url) {
    GURL url(request_url);
    EXPECT_CALL(*api_call_flow_,
                StartPostRequest(url, _, shared_factory_, kAccessToken, _, _))
        .WillOnce(DoAll(SaveArg<1>(&serialized_request_),
                        SaveArg<4>(&flow_result_callback_),
                        SaveArg<5>(&flow_error_callback_)));
  }

  // Sets up an expectation and captures a CryptAuth API GET request to
  // |request_url|.
  void ExpectGetRequest(const std::string& request_url) {
    GURL url(request_url);
    EXPECT_CALL(*api_call_flow_,
                StartGetRequest(url, _, shared_factory_, kAccessToken, _, _))
        .WillOnce(DoAll(SaveArg<1>(&request_as_query_parameters_),
                        SaveArg<4>(&flow_result_callback_),
                        SaveArg<5>(&flow_error_callback_)));
  }

  // Returns |response_proto| as the result to the current API request.
  // ExpectResult() must have been called first.
  void FinishApiCallFlow(const google::protobuf::MessageLite* response_proto) {
    flow_result_callback_.Run(response_proto->SerializeAsString());
  }

  // Ends the current API request with |error|. ExpectResult() must have been
  // called first.
  void FailApiCallFlow(NetworkRequestError error) {
    flow_error_callback_.Run(error);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  // Owned by |client_|.
  StrictMock<MockCryptAuthApiCallFlow>* api_call_flow_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  std::unique_ptr<CryptAuthClient> client_;

  std::string serialized_request_;
  std::vector<std::pair<std::string, std::string>> request_as_query_parameters_;
  CryptAuthApiCallFlow::ResultCallback flow_result_callback_;
  CryptAuthApiCallFlow::ErrorCallback flow_error_callback_;
};

TEST_F(DeviceSyncCryptAuthClientTest, GetMyDevicesSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  cryptauth::GetMyDevicesResponse result_proto;
  cryptauth::GetMyDevicesRequest request_proto;
  request_proto.set_allow_stale_read(true);
  client_->GetMyDevices(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::GetMyDevicesResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::GetMyDevicesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_TRUE(expected_request.allow_stale_read());

  // Return two devices, one unlock key and one unlockable device.
  {
    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    response_proto.mutable_devices(0)->set_unlock_key(true);
    response_proto.mutable_devices(0)->set_bluetooth_address(
        kBluetoothAddress1);
    response_proto.add_devices();
    response_proto.mutable_devices(1)->set_public_key(kPublicKey2);
    response_proto.mutable_devices(1)->set_unlockable(true);
    FinishApiCallFlow(&response_proto);
  }

  // Check that the result received in callback is the same as the response.
  ASSERT_EQ(2, result_proto.devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
  EXPECT_TRUE(result_proto.devices(0).unlock_key());
  EXPECT_EQ(kBluetoothAddress1, result_proto.devices(0).bluetooth_address());
  EXPECT_EQ(kPublicKey2, result_proto.devices(1).public_key());
  EXPECT_TRUE(result_proto.devices(1).unlockable());
}

TEST_F(DeviceSyncCryptAuthClientTest, GetMyDevicesFailure) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  NetworkRequestError error;
  client_->GetMyDevices(
      cryptauth::GetMyDevicesRequest(),
      base::Bind(&NotCalledConstRef<cryptauth::GetMyDevicesResponse>),
      base::Bind(&SaveResult<NetworkRequestError>, &error),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  FailApiCallFlow(NetworkRequestError::kInternalServerError);
  EXPECT_EQ(NetworkRequestError::kInternalServerError, error);
}

TEST_F(DeviceSyncCryptAuthClientTest, FindEligibleUnlockDevicesSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "findeligibleunlockdevices");

  cryptauth::FindEligibleUnlockDevicesResponse result_proto;
  cryptauth::FindEligibleUnlockDevicesRequest request_proto;
  request_proto.set_callback_bluetooth_address(kBluetoothAddress2);
  client_->FindEligibleUnlockDevices(
      request_proto,
      base::Bind(
          &SaveResultConstRef<cryptauth::FindEligibleUnlockDevicesResponse>,
          &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::FindEligibleUnlockDevicesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kBluetoothAddress2, expected_request.callback_bluetooth_address());

  // Return a response proto with one eligible and one ineligible device.
  cryptauth::FindEligibleUnlockDevicesResponse response_proto;
  response_proto.add_eligible_devices();
  response_proto.mutable_eligible_devices(0)->set_public_key(kPublicKey1);

  const cryptauth::IneligibilityReason kIneligibilityReason =
      cryptauth::IneligibilityReason::UNKNOWN_INELIGIBILITY_REASON;
  response_proto.add_ineligible_devices();
  response_proto.mutable_ineligible_devices(0)
      ->mutable_device()
      ->set_public_key(kPublicKey2);
  response_proto.mutable_ineligible_devices(0)->add_reasons(
      kIneligibilityReason);
  FinishApiCallFlow(&response_proto);

  // Check that the result received in callback is the same as the response.
  ASSERT_EQ(1, result_proto.eligible_devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.eligible_devices(0).public_key());
  ASSERT_EQ(1, result_proto.ineligible_devices_size());
  EXPECT_EQ(kPublicKey2,
            result_proto.ineligible_devices(0).device().public_key());
  ASSERT_EQ(1, result_proto.ineligible_devices(0).reasons_size());
  EXPECT_EQ(kIneligibilityReason,
            result_proto.ineligible_devices(0).reasons(0));
}

TEST_F(DeviceSyncCryptAuthClientTest, FindEligibleUnlockDevicesFailure) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "findeligibleunlockdevices");

  NetworkRequestError error;
  cryptauth::FindEligibleUnlockDevicesRequest request_proto;
  request_proto.set_callback_bluetooth_address(kBluetoothAddress1);
  client_->FindEligibleUnlockDevices(
      request_proto,
      base::Bind(
          &NotCalledConstRef<cryptauth::FindEligibleUnlockDevicesResponse>),
      base::Bind(&SaveResult<NetworkRequestError>, &error));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  FailApiCallFlow(NetworkRequestError::kAuthenticationError);
  EXPECT_EQ(NetworkRequestError::kAuthenticationError, error);
}

TEST_F(DeviceSyncCryptAuthClientTest, FindEligibleForPromotionSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "findeligibleforpromotion");

  cryptauth::FindEligibleForPromotionResponse result_proto;
  client_->FindEligibleForPromotion(
      cryptauth::FindEligibleForPromotionRequest(),
      base::Bind(
          &SaveResultConstRef<cryptauth::FindEligibleForPromotionResponse>,
          &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::FindEligibleForPromotionRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));

  cryptauth::FindEligibleForPromotionResponse response_proto;
  FinishApiCallFlow(&response_proto);
}

TEST_F(DeviceSyncCryptAuthClientTest, SendDeviceSyncTickleSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "senddevicesynctickle");

  cryptauth::SendDeviceSyncTickleResponse result_proto;
  client_->SendDeviceSyncTickle(
      cryptauth::SendDeviceSyncTickleRequest(),
      base::Bind(&SaveResultConstRef<cryptauth::SendDeviceSyncTickleResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::SendDeviceSyncTickleRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));

  cryptauth::SendDeviceSyncTickleResponse response_proto;
  FinishApiCallFlow(&response_proto);
}

TEST_F(DeviceSyncCryptAuthClientTest, ToggleEasyUnlockSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "toggleeasyunlock");

  cryptauth::ToggleEasyUnlockResponse result_proto;
  cryptauth::ToggleEasyUnlockRequest request_proto;
  request_proto.set_enable(true);
  request_proto.set_apply_to_all(false);
  request_proto.set_public_key(kPublicKey1);
  client_->ToggleEasyUnlock(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::ToggleEasyUnlockResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::ToggleEasyUnlockRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_TRUE(expected_request.enable());
  EXPECT_EQ(kPublicKey1, expected_request.public_key());
  EXPECT_FALSE(expected_request.apply_to_all());

  cryptauth::ToggleEasyUnlockResponse response_proto;
  FinishApiCallFlow(&response_proto);
}

TEST_F(DeviceSyncCryptAuthClientTest, SetupEnrollmentSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/enrollment/"
      "setup");

  std::string kApplicationId = "mkaes";
  std::vector<std::string> supported_protocols;
  supported_protocols.push_back("gcmV1");
  supported_protocols.push_back("testProtocol");

  cryptauth::SetupEnrollmentResponse result_proto;
  cryptauth::SetupEnrollmentRequest request_proto;
  request_proto.set_application_id(kApplicationId);
  request_proto.add_types("gcmV1");
  request_proto.add_types("testProtocol");
  client_->SetupEnrollment(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::SetupEnrollmentResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::SetupEnrollmentRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kApplicationId, expected_request.application_id());
  ASSERT_EQ(2, expected_request.types_size());
  EXPECT_EQ("gcmV1", expected_request.types(0));
  EXPECT_EQ("testProtocol", expected_request.types(1));

  // Return a fake enrollment session.
  {
    cryptauth::SetupEnrollmentResponse response_proto;
    response_proto.set_status("OK");
    response_proto.add_infos();
    response_proto.mutable_infos(0)->set_type("gcmV1");
    response_proto.mutable_infos(0)->set_enrollment_session_id("session_id");
    response_proto.mutable_infos(0)->set_server_ephemeral_key("ephemeral_key");
    FinishApiCallFlow(&response_proto);
  }

  // Check that the returned proto is the same as the one just created.
  EXPECT_EQ("OK", result_proto.status());
  ASSERT_EQ(1, result_proto.infos_size());
  EXPECT_EQ("gcmV1", result_proto.infos(0).type());
  EXPECT_EQ("session_id", result_proto.infos(0).enrollment_session_id());
  EXPECT_EQ("ephemeral_key", result_proto.infos(0).server_ephemeral_key());
}

TEST_F(DeviceSyncCryptAuthClientTest, FinishEnrollmentSuccess) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/enrollment/"
      "finish");

  static const char kEnrollmentSessionId[] = "enrollment_session_id";
  static const char kEnrollmentMessage[] = "enrollment_message";
  static const char kDeviceEphemeralKey[] = "device_ephermal_key";
  cryptauth::FinishEnrollmentResponse result_proto;
  cryptauth::FinishEnrollmentRequest request_proto;
  request_proto.set_enrollment_session_id(kEnrollmentSessionId);
  request_proto.set_enrollment_message(kEnrollmentMessage);
  request_proto.set_device_ephemeral_key(kDeviceEphemeralKey);
  client_->FinishEnrollment(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::FinishEnrollmentResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauth::FinishEnrollmentRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kEnrollmentSessionId, expected_request.enrollment_session_id());
  EXPECT_EQ(kEnrollmentMessage, expected_request.enrollment_message());
  EXPECT_EQ(kDeviceEphemeralKey, expected_request.device_ephemeral_key());

  {
    cryptauth::FinishEnrollmentResponse response_proto;
    response_proto.set_status("OK");
    FinishApiCallFlow(&response_proto);
  }
  EXPECT_EQ("OK", result_proto.status());
}

TEST_F(DeviceSyncCryptAuthClientTest, SyncKeysSuccess) {
  ExpectPostRequest(
      "https://cryptauthenrollment.testgoogleapis.com/v1:syncKeys");

  static const char kApplicationName[] = "application_name";
  static const char kRandomSessionId[] = "random_session_id";

  cryptauthv2::SyncKeysRequest request_proto;
  request_proto.set_application_name(kApplicationName);

  cryptauthv2::SyncKeysResponse result_proto;
  client_->SyncKeys(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauthv2::SyncKeysResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauthv2::SyncKeysRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kApplicationName, expected_request.application_name());

  {
    cryptauthv2::SyncKeysResponse response_proto;
    response_proto.set_random_session_id(kRandomSessionId);
    FinishApiCallFlow(&response_proto);
  }
  EXPECT_EQ(kRandomSessionId, result_proto.random_session_id());
}

TEST_F(DeviceSyncCryptAuthClientTest, EnrollKeysSuccess) {
  ExpectPostRequest(
      "https://cryptauthenrollment.testgoogleapis.com/v1:enrollKeys");

  static const char kRandomSessionId[] = "random_session_id";
  static const char kCertificateName[] = "certificate_name";

  cryptauthv2::EnrollKeysRequest request_proto;
  request_proto.set_random_session_id(kRandomSessionId);

  cryptauthv2::EnrollKeysResponse result_proto;
  client_->EnrollKeys(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauthv2::EnrollKeysResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauthv2::EnrollKeysRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kRandomSessionId, expected_request.random_session_id());

  {
    cryptauthv2::EnrollKeysResponse response_proto;
    response_proto.add_enroll_single_key_responses()
        ->add_certificate()
        ->set_common_name(kCertificateName);
    FinishApiCallFlow(&response_proto);
  }
  ASSERT_EQ(1, result_proto.enroll_single_key_responses_size());
  ASSERT_EQ(1, result_proto.enroll_single_key_responses(0).certificate_size());
  EXPECT_EQ(
      kCertificateName,
      result_proto.enroll_single_key_responses(0).certificate(0).common_name());
}

TEST_F(DeviceSyncCryptAuthClientTest, SyncMetadataSuccess) {
  ExpectPostRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:syncMetadata");

  static const char kMyDeviceEncryptedMetadata[] = "my_encrypted_metadata";
  static const char kOtherDeviceEncryptedMetadata[] =
      "other_device_encrypted_metadata";
  static const char kMyDeviceName[] = "my_device";
  static const char kOtherDeviceName[] = "other_device";

  cryptauthv2::SyncMetadataRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::GetRequestContextForTest());
  request.set_group_public_key(kPublicKey1);
  request.set_need_group_private_key(true);

  cryptauthv2::SyncMetadataResponse result;
  client_->SyncMetadata(
      request,
      base::Bind(&SaveResultConstRef<cryptauthv2::SyncMetadataResponse>,
                 &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauthv2::SyncMetadataRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(cryptauthv2::GetRequestContextForTest().SerializeAsString(),
            expected_request.context().SerializeAsString());
  EXPECT_EQ(kPublicKey1, expected_request.group_public_key());
  EXPECT_TRUE(expected_request.need_group_private_key());

  {
    cryptauthv2::SyncMetadataResponse response;

    cryptauthv2::DeviceMetadataPacket* metadata =
        response.add_encrypted_metadata();
    metadata->set_device_id(kDeviceId1);
    metadata->set_device_name(kMyDeviceName);
    metadata->set_encrypted_metadata(kMyDeviceEncryptedMetadata);

    metadata = response.add_encrypted_metadata();
    metadata->set_device_id(kDeviceId2);
    metadata->set_device_name(kOtherDeviceName);
    metadata->set_encrypted_metadata(kOtherDeviceEncryptedMetadata);

    response.mutable_client_directive()->CopyFrom(
        cryptauthv2::GetClientDirectiveForTest());

    FinishApiCallFlow(&response);
  }

  ASSERT_EQ(2, result.encrypted_metadata_size());
  EXPECT_EQ(kDeviceId1, result.encrypted_metadata(0).device_id());
  EXPECT_EQ(kMyDeviceName, result.encrypted_metadata(0).device_name());
  EXPECT_EQ(kMyDeviceEncryptedMetadata,
            result.encrypted_metadata(0).encrypted_metadata());
  EXPECT_EQ(kDeviceId2, result.encrypted_metadata(1).device_id());
  EXPECT_EQ(kOtherDeviceName, result.encrypted_metadata(1).device_name());
  EXPECT_EQ(kOtherDeviceEncryptedMetadata,
            result.encrypted_metadata(1).encrypted_metadata());
  EXPECT_EQ(cryptauthv2::GetClientDirectiveForTest().SerializeAsString(),
            result.client_directive().SerializeAsString());
}

TEST_F(DeviceSyncCryptAuthClientTest, ShareGroupPrivateKeySuccess) {
  ExpectPostRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:shareGroupPrivateKey");

  cryptauthv2::EncryptedGroupPrivateKey encrypted_group_private_key;
  encrypted_group_private_key.set_recipient_device_id(kDeviceId1);
  encrypted_group_private_key.set_sender_device_id(kDeviceId2);
  encrypted_group_private_key.set_encrypted_private_key(
      "encrypted_group_private_key");

  cryptauthv2::ShareGroupPrivateKeyRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::GetRequestContextForTest());
  request.add_encrypted_group_private_keys()->CopyFrom(
      encrypted_group_private_key);

  cryptauthv2::ShareGroupPrivateKeyResponse result;
  client_->ShareGroupPrivateKey(
      request,
      base::Bind(&SaveResultConstRef<cryptauthv2::ShareGroupPrivateKeyResponse>,
                 &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauthv2::ShareGroupPrivateKeyRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(cryptauthv2::GetRequestContextForTest().SerializeAsString(),
            expected_request.context().SerializeAsString());
  ASSERT_EQ(1, expected_request.encrypted_group_private_keys_size());
  EXPECT_EQ(
      encrypted_group_private_key.SerializeAsString(),
      expected_request.encrypted_group_private_keys(0).SerializeAsString());

  {
    cryptauthv2::ShareGroupPrivateKeyResponse response;
    FinishApiCallFlow(&response);
  }

  EXPECT_TRUE(result.SerializeAsString().empty());
}

TEST_F(DeviceSyncCryptAuthClientTest, BatchNotifyGroupDevicesSuccess) {
  ExpectGetRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:batchNotifyGroupDevices");

  cryptauthv2::BatchNotifyGroupDevicesRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::BuildRequestContext(
      cryptauthv2::kTestDeviceSyncGroupName,
      BuildClientMetadata(2 /* retry_count */,
                          cryptauthv2::ClientMetadata::MANUAL,
                          kClientMetadataSessionId),
      cryptauthv2::kTestInstanceId, cryptauthv2::kTestInstanceIdToken));
  request.add_notify_device_ids(kDeviceId1);
  request.add_notify_device_ids(kDeviceId2);
  request.set_target_service(cryptauthv2::TargetService::DEVICE_SYNC);
  request.set_feature_type(kFeatureType1);

  cryptauthv2::BatchNotifyGroupDevicesResponse result;
  client_->BatchNotifyGroupDevices(
      request,
      base::Bind(
          &SaveResultConstRef<cryptauthv2::BatchNotifyGroupDevicesResponse>,
          &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  std::vector<std::pair<std::string, std::string>>
      expected_request_as_query_parameters = {
          {"context.client_metadata.retry_count", "2"},
          {"context.client_metadata.invocation_reason",
           base::NumberToString(cryptauthv2::ClientMetadata::MANUAL)},
          {"context.client_metadata.session_id", kClientMetadataSessionId},
          {"context.group", cryptauthv2::kTestDeviceSyncGroupName},
          {"context.device_id", cryptauthv2::kTestInstanceId},
          {"context.device_id_token", cryptauthv2::kTestInstanceIdToken},
          {"notify_device_ids", kDeviceId1},
          {"notify_device_ids", kDeviceId2},
          {"target_service",
           base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC)},
          {"feature_type", kFeatureType1}};
  EXPECT_EQ(expected_request_as_query_parameters, request_as_query_parameters_);

  {
    cryptauthv2::BatchNotifyGroupDevicesResponse response;
    FinishApiCallFlow(&response);
  }

  EXPECT_TRUE(result.SerializeAsString().empty());
}

TEST_F(DeviceSyncCryptAuthClientTest, BatchGetFeatureStatusesSuccess) {
  ExpectGetRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:batchGetFeatureStatuses");

  cryptauthv2::BatchGetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::BuildRequestContext(
      cryptauthv2::kTestDeviceSyncGroupName,
      BuildClientMetadata(2 /* retry_count */,
                          cryptauthv2::ClientMetadata::MANUAL,
                          kClientMetadataSessionId),
      cryptauthv2::kTestInstanceId, cryptauthv2::kTestInstanceIdToken));
  request.add_device_ids(kDeviceId1);
  request.add_device_ids(kDeviceId2);
  request.add_feature_types(kFeatureType1);
  request.add_feature_types(kFeatureType2);

  cryptauthv2::BatchGetFeatureStatusesResponse result;
  client_->BatchGetFeatureStatuses(
      request,
      base::Bind(
          &SaveResultConstRef<cryptauthv2::BatchGetFeatureStatusesResponse>,
          &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  std::vector<std::pair<std::string, std::string>>
      expected_request_as_query_parameters = {
          {"context.client_metadata.retry_count", "2"},
          {"context.client_metadata.invocation_reason",
           base::NumberToString(cryptauthv2::ClientMetadata::MANUAL)},
          {"context.client_metadata.session_id", kClientMetadataSessionId},
          {"context.group", cryptauthv2::kTestDeviceSyncGroupName},
          {"context.device_id", cryptauthv2::kTestInstanceId},
          {"context.device_id_token", cryptauthv2::kTestInstanceIdToken},
          {"device_ids", kDeviceId1},
          {"device_ids", kDeviceId2},
          {"feature_types", kFeatureType1},
          {"feature_types", kFeatureType2}};
  EXPECT_EQ(expected_request_as_query_parameters, request_as_query_parameters_);

  {
    cryptauthv2::BatchGetFeatureStatusesResponse response;
    response.add_device_feature_statuses()->CopyFrom(
        cryptauthv2::BuildDeviceFeatureStatus(
            kDeviceId1, {{kFeatureType1, true /* enabled */},
                         {kFeatureType2, true /* enabled */}}));
    response.add_device_feature_statuses()->CopyFrom(
        cryptauthv2::BuildDeviceFeatureStatus(
            kDeviceId2, {{kFeatureType1, false /* enabled */},
                         {kFeatureType2, false /* enabled */}}));

    FinishApiCallFlow(&response);
  }

  ASSERT_EQ(2, result.device_feature_statuses_size());

  EXPECT_EQ(kDeviceId1, result.device_feature_statuses(0).device_id());
  ASSERT_EQ(2, result.device_feature_statuses(0).feature_statuses_size());
  EXPECT_EQ(
      kFeatureType1,
      result.device_feature_statuses(0).feature_statuses(0).feature_type());
  EXPECT_TRUE(result.device_feature_statuses(0).feature_statuses(0).enabled());
  EXPECT_EQ(
      kFeatureType2,
      result.device_feature_statuses(0).feature_statuses(1).feature_type());
  EXPECT_TRUE(result.device_feature_statuses(0).feature_statuses(1).enabled());

  EXPECT_EQ(kDeviceId2, result.device_feature_statuses(1).device_id());
  ASSERT_EQ(2, result.device_feature_statuses(1).feature_statuses_size());
  EXPECT_EQ(
      kFeatureType1,
      result.device_feature_statuses(1).feature_statuses(0).feature_type());
  EXPECT_FALSE(result.device_feature_statuses(1).feature_statuses(0).enabled());
  EXPECT_EQ(
      kFeatureType2,
      result.device_feature_statuses(1).feature_statuses(1).feature_type());
  EXPECT_FALSE(result.device_feature_statuses(1).feature_statuses(1).enabled());
}

TEST_F(DeviceSyncCryptAuthClientTest, BatchSetFeatureStatusesSuccess) {
  ExpectPostRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:batchSetFeatureStatuses");

  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::GetRequestContextForTest());
  request.add_device_feature_statuses()->CopyFrom(
      cryptauthv2::BuildDeviceFeatureStatus(
          kDeviceId1, {{kFeatureType1, true /* enabled */},
                       {kFeatureType2, true /* enabled */}}));
  request.add_device_feature_statuses()->CopyFrom(
      cryptauthv2::BuildDeviceFeatureStatus(
          kDeviceId2, {{kFeatureType1, false /* enabled */},
                       {kFeatureType2, false /* enabled */}}));
  request.set_enable_exclusively(true);

  cryptauthv2::BatchSetFeatureStatusesResponse result;
  client_->BatchSetFeatureStatuses(
      request,
      base::Bind(
          &SaveResultConstRef<cryptauthv2::BatchSetFeatureStatusesResponse>,
          &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  cryptauthv2::BatchSetFeatureStatusesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(cryptauthv2::GetRequestContextForTest().SerializeAsString(),
            expected_request.context().SerializeAsString());

  ASSERT_EQ(2, expected_request.device_feature_statuses_size());

  EXPECT_EQ(kDeviceId1,
            expected_request.device_feature_statuses(0).device_id());
  ASSERT_EQ(
      2, expected_request.device_feature_statuses(0).feature_statuses_size());
  EXPECT_EQ(kFeatureType1, expected_request.device_feature_statuses(0)
                               .feature_statuses(0)
                               .feature_type());
  EXPECT_TRUE(expected_request.device_feature_statuses(0)
                  .feature_statuses(0)
                  .enabled());
  EXPECT_EQ(kFeatureType2, expected_request.device_feature_statuses(0)
                               .feature_statuses(1)
                               .feature_type());
  EXPECT_TRUE(expected_request.device_feature_statuses(0)
                  .feature_statuses(1)
                  .enabled());

  EXPECT_EQ(kDeviceId2,
            expected_request.device_feature_statuses(1).device_id());
  ASSERT_EQ(
      2, expected_request.device_feature_statuses(1).feature_statuses_size());
  EXPECT_EQ(kFeatureType1, expected_request.device_feature_statuses(1)
                               .feature_statuses(0)
                               .feature_type());
  EXPECT_FALSE(expected_request.device_feature_statuses(1)
                   .feature_statuses(0)
                   .enabled());
  EXPECT_EQ(kFeatureType2, expected_request.device_feature_statuses(1)
                               .feature_statuses(1)
                               .feature_type());
  EXPECT_FALSE(expected_request.device_feature_statuses(1)
                   .feature_statuses(1)
                   .enabled());

  {
    cryptauthv2::BatchSetFeatureStatusesResponse response;
    FinishApiCallFlow(&response);
  }

  EXPECT_TRUE(result.SerializeAsString().empty());
}

TEST_F(DeviceSyncCryptAuthClientTest, GetDevicesActivityStatusSuccess) {
  ExpectGetRequest(
      "https://cryptauthdevicesync.testgoogleapis.com/"
      "v1:getDevicesActivityStatus");

  cryptauthv2::GetDevicesActivityStatusRequest request;
  request.mutable_context()->CopyFrom(cryptauthv2::BuildRequestContext(
      cryptauthv2::kTestDeviceSyncGroupName,
      BuildClientMetadata(2 /* retry_count */,
                          cryptauthv2::ClientMetadata::MANUAL,
                          kClientMetadataSessionId),
      cryptauthv2::kTestInstanceId, cryptauthv2::kTestInstanceIdToken));

  cryptauthv2::GetDevicesActivityStatusResponse result;
  client_->GetDevicesActivityStatus(
      request,
      base::Bind(
          &SaveResultConstRef<cryptauthv2::GetDevicesActivityStatusResponse>,
          &result),
      base::Bind(&NotCalled<NetworkRequestError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  std::vector<std::pair<std::string, std::string>>
      expected_request_as_query_parameters = {
          {"context.client_metadata.retry_count", "2"},
          {"context.client_metadata.invocation_reason",
           base::NumberToString(cryptauthv2::ClientMetadata::MANUAL)},
          {"context.client_metadata.session_id", kClientMetadataSessionId},
          {"context.group", cryptauthv2::kTestDeviceSyncGroupName},
          {"context.device_id", cryptauthv2::kTestInstanceId},
          {"context.device_id_token", cryptauthv2::kTestInstanceIdToken}};
  EXPECT_EQ(expected_request_as_query_parameters, request_as_query_parameters_);

  {
    cryptauthv2::GetDevicesActivityStatusResponse response;
    response.add_device_activity_statuses()->CopyFrom(
        cryptauthv2::BuildDeviceActivityStatus(
            kDeviceId1, kLastActivityTimeSecs1, kConnectivityStatus1));
    response.add_device_activity_statuses()->CopyFrom(
        cryptauthv2::BuildDeviceActivityStatus(
            kDeviceId2, kLastActivityTimeSecs2, kConnectivityStatus2));

    FinishApiCallFlow(&response);
  }

  ASSERT_EQ(2, result.device_activity_statuses_size());

  EXPECT_EQ(kDeviceId1, result.device_activity_statuses(0).device_id());
  ASSERT_EQ(kLastActivityTimeSecs1,
            result.device_activity_statuses(0).last_activity_time_sec());
  EXPECT_EQ(kConnectivityStatus1,
            result.device_activity_statuses(0).connectivity_status());
  EXPECT_EQ(kDeviceId2, result.device_activity_statuses(1).device_id());
  ASSERT_EQ(kLastActivityTimeSecs2,
            result.device_activity_statuses(1).last_activity_time_sec());
  EXPECT_EQ(kConnectivityStatus2,
            result.device_activity_statuses(1).connectivity_status());
}

TEST_F(DeviceSyncCryptAuthClientTest, FetchAccessTokenFailure) {
  NetworkRequestError error;
  client_->GetMyDevices(
      cryptauth::GetMyDevicesRequest(),
      base::Bind(&NotCalledConstRef<cryptauth::GetMyDevicesResponse>),
      base::Bind(&SaveResult<NetworkRequestError>, &error),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_EQ(NetworkRequestError::kAuthenticationError, error);
}

TEST_F(DeviceSyncCryptAuthClientTest, ParseResponseProtoFailure) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  NetworkRequestError error;
  client_->GetMyDevices(
      cryptauth::GetMyDevicesRequest(),
      base::Bind(&NotCalledConstRef<cryptauth::GetMyDevicesResponse>),
      base::Bind(&SaveResult<NetworkRequestError>, &error),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  flow_result_callback_.Run("Not a valid serialized response message.");
  EXPECT_EQ(NetworkRequestError::kResponseMalformed, error);
}

TEST_F(DeviceSyncCryptAuthClientTest,
       MakeSecondRequestBeforeFirstRequestSucceeds) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  // Make first request.
  cryptauth::GetMyDevicesResponse result_proto;
  client_->GetMyDevices(
      cryptauth::GetMyDevicesRequest(),
      base::Bind(&SaveResultConstRef<cryptauth::GetMyDevicesResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  // With request pending, make second request.
  {
    NetworkRequestError error;
    EXPECT_DCHECK_DEATH(client_->FindEligibleUnlockDevices(
        cryptauth::FindEligibleUnlockDevicesRequest(),
        base::Bind(
            &NotCalledConstRef<cryptauth::FindEligibleUnlockDevicesResponse>),
        base::Bind(&SaveResult<NetworkRequestError>, &error)));
  }

  // Complete first request.
  {
    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    FinishApiCallFlow(&response_proto);
  }

  ASSERT_EQ(1, result_proto.devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
}

TEST_F(DeviceSyncCryptAuthClientTest,
       MakeSecondRequestAfterFirstRequestSucceeds) {
  // Make first request successfully.
  {
    ExpectPostRequest(
        "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
        "getmydevices");
    cryptauth::GetMyDevicesResponse result_proto;
    client_->GetMyDevices(
        cryptauth::GetMyDevicesRequest(),
        base::Bind(&SaveResultConstRef<cryptauth::GetMyDevicesResponse>,
                   &result_proto),
        base::Bind(&NotCalled<NetworkRequestError>),
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kAccessToken, base::Time::Max());

    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    FinishApiCallFlow(&response_proto);
    ASSERT_EQ(1, result_proto.devices_size());
    EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
  }

  // Second request fails.
  {
    NetworkRequestError error;
    EXPECT_DCHECK_DEATH(client_->FindEligibleUnlockDevices(
        cryptauth::FindEligibleUnlockDevicesRequest(),
        base::Bind(
            &NotCalledConstRef<cryptauth::FindEligibleUnlockDevicesResponse>),
        base::Bind(&SaveResult<NetworkRequestError>, &error)));
  }
}

TEST_F(DeviceSyncCryptAuthClientTest, DeviceClassifierIsSet) {
  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  cryptauth::GetMyDevicesResponse result_proto;
  cryptauth::GetMyDevicesRequest request_proto;
  request_proto.set_allow_stale_read(true);
  client_->GetMyDevices(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::GetMyDevicesResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  cryptauth::GetMyDevicesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));

  const cryptauth::DeviceClassifier& device_classifier =
      expected_request.device_classifier();
  EXPECT_EQ(kDeviceOsVersionCode, device_classifier.device_os_version_code());
  EXPECT_EQ(kDeviceSoftwareVersionCode,
            device_classifier.device_software_version_code());
  EXPECT_EQ(kDeviceSoftwarePackage,
            device_classifier.device_software_package());
  EXPECT_EQ(kDeviceType,
            DeviceTypeStringToEnum(device_classifier.device_type()));
}

TEST_F(DeviceSyncCryptAuthClientTest, GetAccessTokenUsed) {
  EXPECT_TRUE(client_->GetAccessTokenUsed().empty());

  ExpectPostRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices");

  cryptauth::GetMyDevicesResponse result_proto;
  cryptauth::GetMyDevicesRequest request_proto;
  request_proto.set_allow_stale_read(true);
  client_->GetMyDevices(
      request_proto,
      base::Bind(&SaveResultConstRef<cryptauth::GetMyDevicesResponse>,
                 &result_proto),
      base::Bind(&NotCalled<NetworkRequestError>),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kAccessToken, client_->GetAccessTokenUsed());
}

}  // namespace device_sync

}  // namespace chromeos
