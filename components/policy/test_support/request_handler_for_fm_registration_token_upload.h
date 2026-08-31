// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_FM_REGISTRATION_TOKEN_UPLOAD_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_FM_REGISTRATION_TOKEN_UPLOAD_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `fm_registration_token_upload`.
class RequestHandlerForFmRegistrationTokenUpload
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForFmRegistrationTokenUpload(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForFmRegistrationTokenUpload(
      const RequestHandlerForFmRegistrationTokenUpload&) = delete;
  RequestHandlerForFmRegistrationTokenUpload& operator=(
      const RequestHandlerForFmRegistrationTokenUpload&) = delete;
  ~RequestHandlerForFmRegistrationTokenUpload() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_FM_REGISTRATION_TOKEN_UPLOAD_H_
