// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_ANDROID_MANAGEMENT_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_ANDROID_MANAGEMENT_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Auth token for Android managed accounts.
extern const char kManagedAuthToken[];
// Auth token for other Android unmanaged accounts.
extern const char kUnmanagedAuthToken[];

// Handler for request type `check_android_management`.
class RequestHandlerForCheckAndroidManagement
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForCheckAndroidManagement(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForCheckAndroidManagement(
      RequestHandlerForCheckAndroidManagement&& handler) = delete;
  RequestHandlerForCheckAndroidManagement& operator=(
      RequestHandlerForCheckAndroidManagement&& handler) = delete;
  ~RequestHandlerForCheckAndroidManagement() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHECK_ANDROID_MANAGEMENT_H_
