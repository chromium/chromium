// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_device_and_user.h"

#include <set>
#include <string>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
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
    // TODO(crbug.com/1289442): Remove this case once the type is correctly set
    // for request type `register`.
    case em::DeviceRegisterRequest::TT:
      allowed_policy_types->insert({dm_protocol::kChromeUserPolicyType});
      break;
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
    case em::DeviceRegisterRequest::IOS_BROWSER:
      allowed_policy_types->insert({dm_protocol::kChromeUserPolicyType});
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

std::unique_ptr<HttpResponse> ValidatePsmFields(
    const em::DeviceRegisterRequest& register_request,
    const PolicyStorage* policy_storage) {
  const PolicyStorage::PsmEntry* psm_entry = policy_storage->GetPsmEntry(
      register_request.brand_code() + "_" + register_request.machine_id());
  if (!psm_entry) {
    return nullptr;
  }

  if (!register_request.has_psm_execution_result() ||
      !register_request.has_psm_determination_timestamp_ms()) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "DeviceRegisterRequest must have all required "
                              "PSM execution fields.");
  }

  if (register_request.psm_execution_result() !=
          psm_entry->psm_execution_result ||
      psm_entry->psm_determination_timestamp !=
          register_request.psm_determination_timestamp_ms()) {
    return CreateHttpResponse(
        net::HTTP_BAD_REQUEST,
        "DeviceRegisterRequest must have all correct PSM execution values");
  }

  return nullptr;
}

std::unique_ptr<HttpResponse> ValidateLicenses(
    const em::DeviceRegisterRequest& register_request,
    const PolicyStorage* policy_storage) {
  bool is_enterprise_license = true;
  if (register_request.has_license_type() &&
      register_request.license_type().license_type() ==
          em::LicenseType_LicenseTypeEnum::LicenseType_LicenseTypeEnum_KIOSK) {
    is_enterprise_license = false;
  }

  if ((is_enterprise_license && policy_storage->has_enterprise_license()) ||
      (!is_enterprise_license && policy_storage->has_kiosk_license())) {
    return nullptr;
  }

  return CreateHttpResponse(net::HTTP_PAYMENT_REQUIRED, "No license.");
}

}  // namespace

RequestHandlerForRegisterDeviceAndUser::RequestHandlerForRegisterDeviceAndUser(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

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
  if (!GetGoogleLoginFromRequest(request, &google_login)) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "User not authorized.");
  }

  const base::flat_set<std::string>& managed_users =
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

  std::unique_ptr<HttpResponse> error_response =
      ValidatePsmFields(register_request, policy_storage());
  if (error_response) {
    return error_response;
  }

  error_response = ValidateLicenses(register_request, policy_storage());
  if (error_response) {
    return error_response;
  }

  return RegisterDeviceAndSendResponse(request, register_request, policy_user);
}

std::unique_ptr<HttpResponse>
RequestHandlerForRegisterDeviceAndUser::RegisterDeviceAndSendResponse(
    const HttpRequest& request,
    const em::DeviceRegisterRequest& register_request,
    const std::string& policy_user) {
  std::string device_id =
      KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID);
  std::string device_token = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string machine_name = base::StringPrintf(
      "%s - %s", register_request.machine_model().c_str(), device_id.c_str());

  ClientStorage::ClientInfo client_info;
  client_info.device_id = device_id;
  client_info.device_token = device_token;
  client_info.machine_name = machine_name;
  if (!policy_user.empty()) {
    client_info.username = policy_user;
  }
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

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
