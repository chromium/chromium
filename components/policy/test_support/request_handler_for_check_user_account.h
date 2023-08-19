// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_USER_ACCOUNT_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_USER_ACCOUNT_H_

#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `enterprise_check`.
class RequestHandlerForCheckUserAccount
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForCheckUserAccount(EmbeddedPolicyTestServer* parent);
  RequestHandlerForCheckUserAccount(
      RequestHandlerForCheckUserAccount&& handler) = delete;
  RequestHandlerForCheckUserAccount& operator=(
      RequestHandlerForCheckUserAccount&& handler) = delete;
  ~RequestHandlerForCheckUserAccount() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  enterprise_management::CheckUserAccountResponse::UserAccountType
  GetUserAccountType(const std::string& user_email) const;

  enterprise_management::CheckUserAccountResponse::EnrollmentNudgeType
  GetEnrollmentNudgePolicy(
      const enterprise_management::CheckUserAccountRequest& request) const;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_USER_ACCOUNT_H_
