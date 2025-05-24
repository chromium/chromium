// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_remote_commands.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForRemoteCommands::RequestHandlerForRemoteCommands(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForRemoteCommands::~RequestHandlerForRemoteCommands() = default;

std::string RequestHandlerForRemoteCommands::RequestType() {
  return dm_protocol::kValueRequestRemoteCommands;
}

std::unique_ptr<HttpResponse> RequestHandlerForRemoteCommands::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementResponse response;
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(
          KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID));
  if (!client_info) {
    return CreateHttpResponse(net::HTTP_GONE, response);
  }

  RemoteCommandsState* state = remote_commands_state();

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::DeviceRemoteCommandRequest remote_command_request =
      device_management_request.remote_command_request();

  if (remote_command_request.has_last_command_unique_id()) {
    state->AddRemoteCommandAcked(
        remote_command_request.last_command_unique_id());
  }

  for (auto result : remote_command_request.command_results()) {
    LOG(INFO) << "remote command result: " << result.command_id() << " "
              << result.result();
    state->AddRemoteCommandResult(result);
  }

  std::vector<em::RemoteCommand> pending_commands =
      state->ExtractPendingRemoteCommands();
  ProcessSecureRemoteCommands(remote_command_request, client_info,
                              pending_commands,
                              response.mutable_remote_command_response());
  LOG(INFO) << "serialized string: " << response.SerializeAsString();

  return CreateHttpResponse(net::HTTP_OK, response);
}

void RequestHandlerForRemoteCommands::ProcessSecureRemoteCommands(
    const em::DeviceRemoteCommandRequest& request,
    const ClientStorage::ClientInfo* client_info,
    const std::vector<em::RemoteCommand>& pending_commands,
    em::DeviceRemoteCommandResponse* response) {
  const em::PolicyFetchRequest::SignatureType signature_type =
      request.signature_type();
  const SignatureProvider* signature_provider =
      policy_storage()->signature_provider();
  const SignatureProvider::SigningKey* signing_key =
      signature_provider->GetCurrentKey();

  for (auto command : pending_commands) {
    LOG(INFO) << "pending command type and id: " << command.type() << " "
              << command.command_id();
    em::SignedData* signed_data = response->add_secure_commands();
    em::RemoteCommand command_copy(command);
    command_copy.set_target_device_id(client_info->device_id);

    em::PolicyData policy_data;
    policy_data.set_policy_type(dm_protocol::kChromeRemoteCommandPolicyType);
    policy_data.set_device_id(client_info->device_id);

    command_copy.SerializeToString(policy_data.mutable_policy_value());

    policy_data.SerializeToString(signed_data->mutable_data());
    signing_key->Sign(signed_data->data(), signature_type,
                      signed_data->mutable_signature());
  }
}
}  // namespace policy
