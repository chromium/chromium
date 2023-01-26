// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_PSM_AUTO_ENROLLMENT_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_PSM_AUTO_ENROLLMENT_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `enterprise_psm_check`.
class RequestHandlerForPsmAutoEnrollment
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  enum PirResponse {
    kPirResponseHasMembership = 1,
    kPirResponseHasNoMembership = 2,
  };

  explicit RequestHandlerForPsmAutoEnrollment(EmbeddedPolicyTestServer* parent);
  RequestHandlerForPsmAutoEnrollment(
      RequestHandlerForPsmAutoEnrollment&& handler) = delete;
  RequestHandlerForPsmAutoEnrollment& operator=(
      RequestHandlerForPsmAutoEnrollment&& handler) = delete;
  ~RequestHandlerForPsmAutoEnrollment() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_PSM_AUTO_ENROLLMENT_H_
