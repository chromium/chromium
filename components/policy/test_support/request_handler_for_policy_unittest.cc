// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_policy.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kDeviceToken[] = "fake_device_token";
constexpr char kMachineName[] = "machine_name";
constexpr char kPolicyInvalidationTopic[] = "policy_invalidation_topic";
constexpr char kUsername[] = "user-for-policy@example.com";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char kPublicAccountEntityId[] = "test_user";
constexpr char kExtensionId[] = "extension_id";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

class RequestHandlerForPolicyTest : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForPolicyTest() = default;
  ~RequestHandlerForPolicyTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestPolicy);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForPolicyTest, HandleRequest_NoDeviceToken) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);
}

TEST_F(RequestHandlerForPolicyTest, HandleRequest_NoRegisteredClient) {
  SetDeviceTokenHeader(kDeviceToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_GONE);
}

TEST_F(RequestHandlerForPolicyTest, HandleRequest_UnmatchedDeviceId) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = "registered-device-token";
  client_info.device_id = kDeviceId;
  client_info.machine_name = kMachineName;
  client_storage()->RegisterClient(std::move(client_info));

  SetDeviceTokenHeader(kDeviceToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_GONE);
}

TEST_F(RequestHandlerForPolicyTest, HandleRequest_InvalidPolicyType) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.machine_name = kMachineName;
  client_storage()->RegisterClient(std::move(client_info));

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type("invalid-policy-type");

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForPolicyTest, HandleRequest_UnauthorizedPolicyType) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.machine_name = kMachineName;
  client_info.allowed_policy_types.insert(dm_protocol::kChromeDevicePolicyType);
  client_storage()->RegisterClient(std::move(client_info));

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type(dm_protocol::kChromeRemoteCommandPolicyType);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForPolicyTest, HandleRequest_Success_NoSignedPolicies) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.machine_name = kMachineName;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  client_storage()->RegisterClient(client_info);

  policy_storage()->set_policy_invalidation_topic(kPolicyInvalidationTopic);

  em::CloudPolicySettings settings;
  settings.mutable_savingbrowserhistorydisabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  settings.mutable_savingbrowserhistorydisabled()->set_value(true);
  policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 1);
  const em::PolicyFetchResponse& fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData policy_data;
  policy_data.ParseFromString(fetch_response.policy_data());
  EXPECT_EQ(policy_data.policy_type(),
            dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  EXPECT_EQ(policy_data.request_token(), client_info.device_token);
  EXPECT_EQ(policy_data.policy_value(),
            policy_storage()->GetPolicyPayload(
                dm_protocol::kChromeMachineLevelUserCloudPolicyType));
  EXPECT_EQ(policy_data.machine_name(), client_info.machine_name);
  EXPECT_FALSE(policy_data.service_account_identity().empty());
  EXPECT_EQ(policy_data.device_id(), client_info.device_id);
  EXPECT_EQ(policy_data.username(), kUsername);
  EXPECT_EQ(policy_data.policy_invalidation_topic(), kPolicyInvalidationTopic);
  EXPECT_FALSE(policy_data.has_public_key_version());
  EXPECT_TRUE(fetch_response.policy_data_signature().empty());
}

class RequestHandlerForPolicyTestWithParametrizedSignatureType
    : public RequestHandlerForPolicyTest,
      public testing::WithParamInterface<
          em::PolicyFetchRequest::SignatureType> {
 public:
  RequestHandlerForPolicyTestWithParametrizedSignatureType() = default;
  ~RequestHandlerForPolicyTestWithParametrizedSignatureType() override =
      default;
  em::PolicyFetchRequest::SignatureType GetSignatureTypeParam() {
    return GetParam();
  }
};

TEST_P(RequestHandlerForPolicyTestWithParametrizedSignatureType,
       HandleRequest_Success_SignedPoliciesWithoutClientKey) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_savingbrowserhistorydisabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  settings.mutable_savingbrowserhistorydisabled()->set_value(true);
  policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  fetch_request->set_signature_type(GetSignatureTypeParam());

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 1);
  const em::PolicyFetchResponse& fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData policy_data;
  policy_data.ParseFromString(fetch_response.policy_data());
  EXPECT_EQ(policy_data.policy_type(),
            dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  EXPECT_EQ(policy_data.policy_value(),
            policy_storage()->GetPolicyPayload(
                dm_protocol::kChromeMachineLevelUserCloudPolicyType));
  EXPECT_EQ(policy_data.public_key_version(),
            policy_storage()->signature_provider()->current_key_version());

  EXPECT_FALSE(fetch_response.policy_data_signature().empty());
  EXPECT_FALSE(fetch_response.new_public_key_verification_signature_deprecated()
                   .empty());
  EXPECT_TRUE(fetch_response.new_public_key_signature().empty());
  EXPECT_EQ(fetch_response.policy_data_signature_type(),
            GetSignatureTypeParam());
}

TEST_P(RequestHandlerForPolicyTestWithParametrizedSignatureType,
       HandleRequest_Success_SendsNewPublicKeyVerificationData) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.machine_name = kMachineName;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_savingbrowserhistorydisabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  settings.mutable_savingbrowserhistorydisabled()->set_value(true);
  policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  fetch_request->set_signature_type(GetSignatureTypeParam());

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 1);
  const em::PolicyFetchResponse& fetch_response =
      device_management_response.policy_response().responses(0);
  EXPECT_TRUE(fetch_response.has_new_public_key());

  em::PolicyData policy_data;
  ASSERT_TRUE(policy_data.ParseFromString(fetch_response.policy_data()));

  em::PublicKeyVerificationData verification_data;
  ASSERT_TRUE(verification_data.ParseFromString(
      fetch_response.new_public_key_verification_data()));
  EXPECT_EQ(verification_data.new_public_key(),
            fetch_response.new_public_key());
  EXPECT_EQ(verification_data.new_public_key_version(),
            policy_data.public_key_version());
  EXPECT_EQ(verification_data.domain(), policy_data.managed_by());

  CloudPolicyValidatorBase::VerifySignature(
      fetch_response.new_public_key_verification_data(),
      policy_storage()->signature_provider()->GetVerificationPublicKey(),
      fetch_response.new_public_key_verification_data_signature(),
      em::PolicyFetchRequest::SHA256_RSA);
}

TEST_P(RequestHandlerForPolicyTestWithParametrizedSignatureType,
       HandleRequest_Success_SignedPoliciesWithClientKey) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_savingbrowserhistorydisabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  settings.mutable_savingbrowserhistorydisabled()->set_value(true);
  policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* fetch_request =
      device_management_request.mutable_policy_request()->add_requests();
  fetch_request->set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  fetch_request->set_signature_type(GetSignatureTypeParam());
  // Sets client key to a key different than the current key in signature
  // provider (1), to force setting |new_public_key_signature| in the fetch
  // response.
  fetch_request->set_public_key_version(2);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 1);
  const em::PolicyFetchResponse& fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData policy_data;
  policy_data.ParseFromString(fetch_response.policy_data());
  EXPECT_EQ(policy_data.policy_type(),
            dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  EXPECT_EQ(policy_data.policy_value(),
            policy_storage()->GetPolicyPayload(
                dm_protocol::kChromeMachineLevelUserCloudPolicyType));
  EXPECT_EQ(policy_data.public_key_version(),
            policy_storage()->signature_provider()->current_key_version());

  EXPECT_FALSE(fetch_response.policy_data_signature().empty());
  EXPECT_FALSE(fetch_response.new_public_key_verification_signature_deprecated()
                   .empty());
  EXPECT_FALSE(fetch_response.new_public_key_signature().empty());
  EXPECT_EQ(fetch_response.policy_data_signature_type(),
            GetSignatureTypeParam());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(RequestHandlerForPolicyTest,
       HandleRequest_Success_UnsignedExtensionPolicies) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeExtensionPolicyType);
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_extensionsettings()->mutable_value()->assign(
      "extension-policy");
  policy_storage()->SetPolicyPayload(dm_protocol::kChromeExtensionPolicyType,
                                     kExtensionId,
                                     settings.SerializeAsString());

  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_policy_request()
      ->add_requests()
      ->set_policy_type(dm_protocol::kChromeExtensionPolicyType);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 1);
  const em::PolicyFetchResponse& fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData policy_data;
  policy_data.ParseFromString(fetch_response.policy_data());
  EXPECT_EQ(policy_data.policy_type(), dm_protocol::kChromeExtensionPolicyType);
  EXPECT_EQ(policy_data.policy_value(),
            policy_storage()->GetPolicyPayload(
                dm_protocol::kChromeExtensionPolicyType, kExtensionId));
}

TEST_F(RequestHandlerForPolicyTest,
       HandleRequest_ExtensionPoliciesWithMissingSigningKey) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_extensionsettings()->mutable_value()->assign(
      "extension-policy");
  policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType, kExtensionId,
      settings.SerializeAsString());
  policy_storage()->signature_provider()->set_current_key_version(-1);

  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_policy_request()
      ->add_requests()
      ->set_policy_type(
          dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_P(RequestHandlerForPolicyTestWithParametrizedSignatureType,
       HandleRequest_ExtensionsInPublicAccounts_SetCorrectPolicyDataUsername) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId;
  client_info.username = kUsername;
  client_info.allowed_policy_types.insert(
      {dm_protocol::kChromeExtensionPolicyType,
       dm_protocol::kChromePublicAccountPolicyType});
  client_storage()->RegisterClient(client_info);

  em::CloudPolicySettings settings;
  settings.mutable_extensionsettings()->mutable_value()->assign(
      "extension-policy");
  policy_storage()->SetPolicyPayload(dm_protocol::kChromeExtensionPolicyType,
                                     kExtensionId,
                                     settings.SerializeAsString());

  std::vector<policy::SignatureProvider::SigningKey> universal_signing_keys;
  universal_signing_keys.push_back(policy::SignatureProvider::SigningKey(
      policy::PolicyBuilder::CreateTestSigningKey(),
      {{"*", policy::PolicyBuilder::GetTestSigningKeySignature()}}));
  policy_storage()->signature_provider()->set_signing_keys(
      std::move(universal_signing_keys));

  em::DeviceManagementRequest device_management_request;
  em::PolicyFetchRequest* extension_request =
      device_management_request.mutable_policy_request()->add_requests();
  extension_request->set_policy_type(dm_protocol::kChromeExtensionPolicyType);
  extension_request->set_signature_type(GetSignatureTypeParam());
  em::PolicyFetchRequest* public_account_request =
      device_management_request.mutable_policy_request()->add_requests();
  public_account_request->set_policy_type(
      dm_protocol::kChromePublicAccountPolicyType);
  public_account_request->set_settings_entity_id(kPublicAccountEntityId);
  public_account_request->set_signature_type(GetSignatureTypeParam());

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();

  ASSERT_EQ(device_management_response.policy_response().responses_size(), 2);
  const em::PolicyFetchResponse& extension_fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData extension_policy_data;
  extension_policy_data.ParseFromString(extension_fetch_response.policy_data());
  EXPECT_EQ(extension_policy_data.username(), kPublicAccountEntityId);
  EXPECT_EQ(extension_fetch_response.policy_data_signature_type(),
            GetSignatureTypeParam());

  const em::PolicyFetchResponse& public_account_fetch_response =
      device_management_response.policy_response().responses(0);
  em::PolicyData public_account_policy_data;
  public_account_policy_data.ParseFromString(
      public_account_fetch_response.policy_data());
  EXPECT_EQ(public_account_policy_data.username(), kPublicAccountEntityId);
  EXPECT_EQ(public_account_fetch_response.policy_data_signature_type(),
            GetSignatureTypeParam());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

INSTANTIATE_TEST_SUITE_P(
    SignatureType,
    RequestHandlerForPolicyTestWithParametrizedSignatureType,
    testing::Values(em::PolicyFetchRequest::SHA1_RSA,
                    em::PolicyFetchRequest::SHA256_RSA));

}  // namespace policy
