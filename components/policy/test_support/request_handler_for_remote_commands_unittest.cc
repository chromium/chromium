// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_remote_commands.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId1[] = "fake_device_id_1";
constexpr char kDeviceId2[] = "fake_device_id_2";
constexpr char kDeviceToken[] = "fake_device_token";
constexpr char kMachineName[] = "machine_name";

}  // namespace

class RequestHandlerForRemoteCommandsTest
    : public EmbeddedPolicyTestServerTestBase {
 public:
  RequestHandlerForRemoteCommandsTest() = default;
  ~RequestHandlerForRemoteCommandsTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestRemoteCommands);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId1);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }

  em::RemoteCommand ValidateSignedData(
      const em::SignedData signed_data,
      const std::string& public_key,
      em::PolicyFetchRequest::SignatureType signature_type) {
    const bool valid_signature = CloudPolicyValidatorBase::VerifySignature(
        signed_data.data(), public_key, signed_data.signature(),
        signature_type);
    em::PolicyData policy_data;
    em::RemoteCommand remote_command;

    EXPECT_TRUE(valid_signature);
    EXPECT_TRUE(policy_data.ParseFromString(signed_data.data()));
    EXPECT_EQ(policy_data.policy_type(),
              dm_protocol::kChromeRemoteCommandPolicyType);
    EXPECT_TRUE(remote_command.ParseFromString(policy_data.policy_value()));
    EXPECT_EQ(remote_command.target_device_id(), kDeviceId1);

    return remote_command;
  }
};

TEST_F(RequestHandlerForRemoteCommandsTest, HandleRequest_UnmatchedDeviceId) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId2;
  client_info.machine_name = kMachineName;
  client_storage()->RegisterClient(std::move(client_info));

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_GONE);
}

TEST_F(RequestHandlerForRemoteCommandsTest, HandleRequest_AcceptCommandResult) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId1;
  client_info.machine_name = kMachineName;
  client_storage()->RegisterClient(std::move(client_info));

  const int64_t command_id = 10;
  em::DeviceManagementRequest device_management_request;
  em::RemoteCommandResult* result =
      device_management_request.mutable_remote_command_request()
          ->add_command_results();
  result->set_command_id(command_id);
  result->set_result(em::RemoteCommandResult::RESULT_SUCCESS);

  SetPayload(device_management_request);
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  EXPECT_TRUE(HasResponseBody());

  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  em::DeviceRemoteCommandResponse remote_command_response =
      device_management_response.remote_command_response();

  EXPECT_EQ(remote_command_response.secure_commands_size(), 0);
  EXPECT_EQ(remote_command_response.commands_size(), 0);

  em::RemoteCommandResult expected_result;
  EXPECT_TRUE(remote_commands_state()->GetRemoteCommandResult(
      command_id, &expected_result));
  EXPECT_EQ(result->result(), expected_result.result());
}

TEST_F(RequestHandlerForRemoteCommandsTest, HandleRequest_HasPendingCommand) {
  ClientStorage::ClientInfo client_info;
  client_info.device_token = kDeviceToken;
  client_info.device_id = kDeviceId1;
  client_info.machine_name = kMachineName;
  client_storage()->RegisterClient(std::move(client_info));

  RemoteCommandsState* state = remote_commands_state();
  std::string payload = "";
  em::RemoteCommand cmd1;
  cmd1.set_command_id(1);
  cmd1.set_payload(payload);
  cmd1.set_type(em::RemoteCommand::COMMAND_ECHO_TEST);
  em::RemoteCommand cmd2;
  cmd2.set_command_id(2);
  cmd2.set_payload(payload);
  cmd2.set_type(em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
  state->AddPendingRemoteCommand(cmd1);
  state->AddPendingRemoteCommand(cmd2);
  em::PolicyFetchRequest::SignatureType signature_type =
      em::PolicyFetchRequest::SHA256_RSA;
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_remote_command_request()
      ->set_signature_type(signature_type);

  SetPayload(device_management_request);
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  EXPECT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  em::DeviceRemoteCommandResponse remote_command_response =
      device_management_response.remote_command_response();

  EXPECT_EQ(remote_command_response.secure_commands_size(), 2);
  EXPECT_EQ(remote_command_response.commands_size(), 0);

  const SignatureProvider* signature_provider =
      policy_storage()->signature_provider();
  const SignatureProvider::SigningKey* signing_key =
      signature_provider->GetCurrentKey();
  std::string public_key = signing_key->public_key();

  em::SignedData first_signed_data =
      remote_command_response.secure_commands()[0];
  em::RemoteCommand first_remote_command = ValidateSignedData(
      first_signed_data, public_key, em::PolicyFetchRequest::SHA256_RSA);
  EXPECT_EQ(first_remote_command.command_id(), cmd1.command_id());
  EXPECT_EQ(first_remote_command.type(), cmd1.type());

  em::SignedData second_signed_data =
      remote_command_response.secure_commands()[1];
  em::RemoteCommand second_remote_command = ValidateSignedData(
      second_signed_data, public_key, em::PolicyFetchRequest::SHA256_RSA);
  EXPECT_EQ(second_remote_command.command_id(), cmd2.command_id());
  EXPECT_EQ(second_remote_command.type(), cmd2.type());
}
}  // namespace policy
