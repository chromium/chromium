// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_BROWSER_PUBLIC_KEY_UPLOAD_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_BROWSER_PUBLIC_KEY_UPLOAD_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

class RequestHandlerForBrowserPublicKeyUpload
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForBrowserPublicKeyUpload(
      EmbeddedPolicyTestServer* parent);

  RequestHandlerForBrowserPublicKeyUpload(
      RequestHandlerForBrowserPublicKeyUpload&& handler) = delete;
  RequestHandlerForBrowserPublicKeyUpload& operator=(
      RequestHandlerForBrowserPublicKeyUpload&& handler) = delete;

  ~RequestHandlerForBrowserPublicKeyUpload() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_BROWSER_PUBLIC_KEY_UPLOAD_H_
