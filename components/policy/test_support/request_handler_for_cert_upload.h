// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CERT_UPLOAD_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CERT_UPLOAD_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

#include <memory>
#include <string>

#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

// Handler for request type `cert_upload`.
class RequestHandlerForCertUpload
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForCertUpload(EmbeddedPolicyTestServer* parent);
  RequestHandlerForCertUpload(RequestHandlerForCertUpload&& handler) = delete;
  RequestHandlerForCertUpload& operator=(
      RequestHandlerForCertUpload&& handler) = delete;
  ~RequestHandlerForCertUpload() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CERT_UPLOAD_H_
