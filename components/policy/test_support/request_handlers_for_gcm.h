// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLERS_FOR_GCM_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLERS_FOR_GCM_H_

#include <memory>
#include <string>

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

inline constexpr char kGcmCheckinRequestType[] = "gcm_checkin";
inline constexpr char kGcmRegisterRequestType[] = "gcm_register";

// Handler for GCM checkin requests.
class RequestHandlerForGcmCheckin
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForGcmCheckin(EmbeddedPolicyTestServer* parent);
  ~RequestHandlerForGcmCheckin() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

// Handler for GCM registration requests.
class RequestHandlerForGcmRegister
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForGcmRegister(EmbeddedPolicyTestServer* parent);
  ~RequestHandlerForGcmRegister() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLERS_FOR_GCM_H_
