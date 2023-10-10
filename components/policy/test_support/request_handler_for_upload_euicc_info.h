// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_UPLOAD_EUICC_INFO_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_UPLOAD_EUICC_INFO_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `upload_euicc_info`.
class RequestHandlerForUploadEuiccInfo
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForUploadEuiccInfo(EmbeddedPolicyTestServer* parent);
  RequestHandlerForUploadEuiccInfo(RequestHandlerForUploadEuiccInfo&& handler) =
      delete;
  RequestHandlerForUploadEuiccInfo& operator=(
      RequestHandlerForUploadEuiccInfo&& handler) = delete;
  ~RequestHandlerForUploadEuiccInfo() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_UPLOAD_EUICC_INFO_H_
