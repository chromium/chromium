// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_policy.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForPolicy::RequestHandlerForPolicy(ClientStorage* client_storage,
                                                 PolicyStorage* policy_storage)
    : EmbeddedPolicyTestServer::RequestHandler(client_storage, policy_storage) {
}

RequestHandlerForPolicy::~RequestHandlerForPolicy() = default;

std::string RequestHandlerForPolicy::RequestType() {
  return dm_protocol::kValueRequestPolicy;
}

std::unique_ptr<HttpResponse> RequestHandlerForPolicy::HandleRequest(
    const HttpRequest& request) {
  const std::set<std::string> kCloudPolicyTypes{
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      dm_protocol::kChromeMachineLevelUserCloudPolicyAndroidType,
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
  };

  std::string request_device_token;
  if (!GetDeviceTokenFromRequest(request, &request_device_token))
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Invalid device token.");

  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(
          KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID));
  if (!client_info || client_info->device_token != request_device_token)
    return CreateHttpResponse(net::HTTP_GONE, "Invalid device token.");

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);

  em::DeviceManagementResponse device_management_response;
  for (const auto& fetch_request :
       device_management_request.policy_request().requests()) {
    const std::string& policy_type = fetch_request.policy_type();
    // TODO(crbug.com/1221328): Add other policy types as needed.
    if (kCloudPolicyTypes.find(fetch_request.policy_type()) ==
        kCloudPolicyTypes.end()) {
      return CreateHttpResponse(
          net::HTTP_BAD_REQUEST,
          base::StringPrintf("Invalid policy_type: %s", policy_type.c_str()));
    }

    std::string error_msg;
    if (!ProcessCloudPolicy(
            policy_type, *client_info,
            device_management_response.mutable_policy_response()
                ->add_responses(),
            &error_msg)) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST, error_msg);
    }
  }

  return CreateHttpResponse(net::HTTP_OK,
                            device_management_response.SerializeAsString());
}

bool RequestHandlerForPolicy::ProcessCloudPolicy(
    const std::string& policy_type,
    const ClientStorage::ClientInfo& client_info,
    em::PolicyFetchResponse* fetch_response,
    std::string* error_msg) {
  if (client_info.allowed_policy_types.find(policy_type) ==
      client_info.allowed_policy_types.end()) {
    error_msg->assign("Request not allowed for the token used");
    return false;
  }

  em::PolicyData policy_data;
  policy_data.set_policy_type(policy_type);
  policy_data.set_timestamp(base::Time::Now().ToJavaTime());
  policy_data.set_request_token(client_info.device_token);
  policy_data.set_policy_value(policy_storage()->GetPolicyPayload(policy_type));
  policy_data.set_machine_name(client_info.machine_name);
  policy_data.set_service_account_identity(
      policy_storage()->service_account_identity().empty()
          ? "policy_testserver.py-service_account_identity@gmail.com"
          : policy_storage()->service_account_identity());
  policy_data.set_device_id(client_info.device_id);
  policy_data.SerializeToString(fetch_response->mutable_policy_data());

  return true;
}

}  // namespace policy
