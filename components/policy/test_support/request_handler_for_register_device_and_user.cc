// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_device_and_user.h"

#include <set>
#include <string>

#include "base/guid.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

namespace {

void AddAllowedPolicyTypes(em::DeviceRegisterRequest::Type type,
                           std::set<std::string>* allowed_policy_types) {
  switch (type) {
    case em::DeviceRegisterRequest::USER:
      allowed_policy_types->insert({dm_protocol::kChromeUserPolicyType,
                                    dm_protocol::kChromeExtensionPolicyType});
      break;
    case em::DeviceRegisterRequest::DEVICE:
      allowed_policy_types->insert(
          {dm_protocol::kChromeDevicePolicyType,
           dm_protocol::kChromePublicAccountPolicyType,
           dm_protocol::kChromeExtensionPolicyType,
           dm_protocol::kChromeSigninExtensionPolicyType});
      break;
    case em::DeviceRegisterRequest::BROWSER:
      allowed_policy_types->insert({dm_protocol::kChromeUserPolicyType,
                                    dm_protocol::kChromeExtensionPolicyType});
      break;
    case em::DeviceRegisterRequest::ANDROID_BROWSER:
      allowed_policy_types->insert({dm_protocol::kChromeUserPolicyType});
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

RequestHandlerForRegisterDeviceAndUser::RequestHandlerForRegisterDeviceAndUser(
    ClientStorage* client_storage,
    PolicyStorage* policy_storage)
    : EmbeddedPolicyTestServer::RequestHandler(client_storage, policy_storage) {
}

RequestHandlerForRegisterDeviceAndUser::
    ~RequestHandlerForRegisterDeviceAndUser() = default;

std::string RequestHandlerForRegisterDeviceAndUser::RequestType() {
  return dm_protocol::kValueRequestRegister;
}

std::unique_ptr<HttpResponse>
RequestHandlerForRegisterDeviceAndUser::HandleRequest(
    const HttpRequest& request) {
  // Only checks the the oauth token is set, but doesn't use it yet. User will
  // be obtained from the policy storage.
  // TODO(http://crbug.com/1227123): Add support for authentication.
  std::string google_login;
  if (!GetGoogleLoginFromRequest(request, &google_login))
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "User not authorized.");

  const std::set<std::string>& managed_users =
      policy_storage()->managed_users();
  if (managed_users.empty()) {
    return CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                              "No managed users.");
  }

  const std::string& policy_user = policy_storage()->policy_user();
  if (managed_users.find("*") == managed_users.end() &&
      managed_users.find(policy_user) == managed_users.end()) {
    return CreateHttpResponse(net::HTTP_FORBIDDEN, "Unmanaged.");
  }

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::DeviceRegisterRequest& register_request =
      device_management_request.register_request();

  std::string device_id =
      KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID);
  std::string device_token = base::GUID::GenerateRandomV4().AsLowercaseString();
  std::string machine_name = base::StringPrintf(
      "%s - %s", register_request.machine_model().c_str(), device_id.c_str());

  ClientStorage::ClientInfo client_info;
  client_info.device_id = device_id;
  client_info.device_token = device_token;
  client_info.machine_name = machine_name;
  if (!policy_user.empty())
    client_info.username = policy_user;
  AddAllowedPolicyTypes(register_request.type(),
                        &client_info.allowed_policy_types);
  client_storage()->RegisterClient(std::move(client_info));

  em::DeviceManagementResponse device_management_response;
  em::DeviceRegisterResponse* register_response =
      device_management_response.mutable_register_response();
  register_response->set_device_management_token(device_token);
  register_response->set_machine_name(machine_name);
  register_response->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);

  return CreateHttpResponse(net::HTTP_OK,
                            device_management_response.SerializeAsString());
}

}  // namespace policy
