// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_POLICY_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_POLICY_H_

#include <string>

#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace enterprise_management {
class DevicePolicyResponse;
class PolicyFetchRequest;
class PolicyFetchResponse;
}  // namespace enterprise_management

namespace policy {

inline constexpr char kDefaultUsername[] = "username@example.com";

// Handler for request type `policy`.
class RequestHandlerForPolicy
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForPolicy(EmbeddedPolicyTestServer* parent);
  RequestHandlerForPolicy(RequestHandlerForPolicy&& handler) = delete;
  RequestHandlerForPolicy& operator=(RequestHandlerForPolicy&& handler) =
      delete;
  ~RequestHandlerForPolicy() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  // Add to |fetch_response| the policies associated with |client| according to
  // |policy_type|. Returns true is request is well-formed, or false otherwise
  // (in which case, |error_msg| is set with the corresponding error message).
  bool ProcessCloudPolicy(
      const enterprise_management::PolicyFetchRequest& fetch_request,
      const ClientStorage::ClientInfo& client,
      enterprise_management::PolicyFetchResponse* fetch_response,
      std::string* error_msg);

  // Add to |response| the policies associated with |client_info| for extension
  // policy type in |fetch_request|. Returns true is request is well-formed, or
  // false otherwise (in which case, |error_msg| is set with the corresponding
  // error message).
  bool ProcessCloudPolicyForExtensions(
      const enterprise_management::PolicyFetchRequest& fetch_request,
      const ClientStorage::ClientInfo& client_info,
      enterprise_management::DevicePolicyResponse* response,
      std::string* error_msg);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_POLICY_H_
