// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_API_AUTHORIZATION_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_API_AUTHORIZATION_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `api_authorization`.
class RequestHandlerForApiAuthorization
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForApiAuthorization(EmbeddedPolicyTestServer* parent);
  RequestHandlerForApiAuthorization(
      RequestHandlerForApiAuthorization&& handler) = delete;
  RequestHandlerForApiAuthorization& operator=(
      RequestHandlerForApiAuthorization&& handler) = delete;
  ~RequestHandlerForApiAuthorization() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_API_AUTHORIZATION_H_
