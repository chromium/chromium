// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_check_user_account.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForCheckUserAccount::RequestHandlerForCheckUserAccount(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForCheckUserAccount::~RequestHandlerForCheckUserAccount() =
    default;

std::string RequestHandlerForCheckUserAccount::RequestType() {
  return dm_protocol::kValueCheckUserAccount;
}

std::unique_ptr<HttpResponse> RequestHandlerForCheckUserAccount::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::CheckUserAccountRequest& check_user_account_request =
      device_management_request.check_user_account_request();

  em::DeviceManagementResponse device_management_response;
  em::CheckUserAccountResponse& check_user_account_response =
      *device_management_response.mutable_check_user_account_response();

  const std::string& user_email = check_user_account_request.user_email();
  check_user_account_response.set_user_account_type(
      GetUserAccountType(user_email));

  check_user_account_response.set_enrollment_nudge_type(
      GetEnrollmentNudgePolicy(check_user_account_request));

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

em::CheckUserAccountResponse::UserAccountType
RequestHandlerForCheckUserAccount::GetUserAccountType(
    const std::string& user_email) const {
  if (policy_storage()->managed_users().contains(user_email)) {
    return em::CheckUserAccountResponse::DASHER;
  }
  return em::CheckUserAccountResponse::CONSUMER;
}

em::CheckUserAccountResponse::EnrollmentNudgeType
RequestHandlerForCheckUserAccount::GetEnrollmentNudgePolicy(
    const enterprise_management::CheckUserAccountRequest& request) const {
  if (!request.has_enrollment_nudge_request() ||
      !request.enrollment_nudge_request()) {
    return em::CheckUserAccountResponse::UNKNOWN_ENROLLMENT_NUDGE_TYPE;
  }
  if (policy_storage()->enrollment_required()) {
    return em::CheckUserAccountResponse::ENROLLMENT_REQUIRED;
  }
  return em::CheckUserAccountResponse::NONE;
}

}  // namespace policy
