// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_ADMIN_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_ADMIN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace policy {

// Handler for /admin requests.
class RequestHandlerForAdmin : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  // Delegate interface for operations that manage policies and push messages.
  // FakeDMServer invokes these methods on its main sequence while handling a
  // request.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the raw JSON content of the policy file, or "{}\n" if it cannot
    // be read.
    virtual std::string GetPoliciesJson() const = 0;

    // Validates `json_content`, writes it to the policy file, and broadcasts a
    // policy invalidation. Returns true on success, or false if `json_content`
    // is invalid or writing the file fails.
    virtual bool UpdatePoliciesJson(const std::string& json_content) = 0;

    // Reloads the policy file and broadcasts a policy invalidation. Returns
    // std::nullopt if the policy file is invalid; otherwise returns true if at
    // least one connected client received the invalidation, or false otherwise.
    virtual std::optional<bool> TriggerPolicyInvalidation() = 0;
  };

  RequestHandlerForAdmin(EmbeddedPolicyTestServer* test_server,
                         Delegate& delegate);
  RequestHandlerForAdmin(RequestHandlerForAdmin&& handler) = delete;
  RequestHandlerForAdmin& operator=(RequestHandlerForAdmin&& handler) = delete;
  ~RequestHandlerForAdmin() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  const raw_ref<Delegate> delegate_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_ADMIN_H_
