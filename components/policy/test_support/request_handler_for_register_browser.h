// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

class RequestHandlerForRegisterBrowserOrPolicyAgent
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForRegisterBrowserOrPolicyAgent(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForRegisterBrowserOrPolicyAgent(
      RequestHandlerForRegisterBrowserOrPolicyAgent&& handler) = delete;
  RequestHandlerForRegisterBrowserOrPolicyAgent& operator=(
      RequestHandlerForRegisterBrowserOrPolicyAgent&& handler) = delete;
  ~RequestHandlerForRegisterBrowserOrPolicyAgent() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 protected:
  // Validates a RegisterBrowserRequest, returning an HTTP response to be served
  // if validation fails. Returns nullptr if validation succeeds.
  virtual std::unique_ptr<net::test_server::HttpResponse>
  ValidateRegisterBrowserRequest(
      const enterprise_management::RegisterBrowserRequest&
          register_browser_request) = 0;

  virtual constexpr base::flat_set<std::string> allowed_policy_types() = 0;
};

// Handler for request type `register_browser`.
class RequestHandlerForRegisterBrowser
    : public RequestHandlerForRegisterBrowserOrPolicyAgent {
 public:
  explicit RequestHandlerForRegisterBrowser(EmbeddedPolicyTestServer* parent);
  RequestHandlerForRegisterBrowser(RequestHandlerForRegisterBrowser&& handler) =
      delete;
  RequestHandlerForRegisterBrowser& operator=(
      RequestHandlerForRegisterBrowser&& handler) = delete;
  ~RequestHandlerForRegisterBrowser() override;

  // RequestHandlerForRegisterBrowserOrPolicyAgent:
  std::string RequestType() override;

 protected:
  // RequestHandlerForRegisterPolicyAgent:
  std::unique_ptr<net::test_server::HttpResponse>
  ValidateRegisterBrowserRequest(
      const enterprise_management::RegisterBrowserRequest&
          register_browser_request) override;

  constexpr base::flat_set<std::string> allowed_policy_types() override;
};

// Handler for request type `register_policy_agent`.
class RequestHandlerForRegisterPolicyAgent
    : public RequestHandlerForRegisterBrowserOrPolicyAgent {
 public:
  explicit RequestHandlerForRegisterPolicyAgent(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForRegisterPolicyAgent(
      RequestHandlerForRegisterPolicyAgent&& handler) = delete;
  RequestHandlerForRegisterPolicyAgent& operator=(
      RequestHandlerForRegisterPolicyAgent&& handler) = delete;
  ~RequestHandlerForRegisterPolicyAgent() override;

  // RequestHandlerForRegisterBrowserOrPolicyAgent:
  std::string RequestType() override;

 protected:
  // RequestHandlerForRegisterBrowserOrPolicyAgent:
  std::unique_ptr<net::test_server::HttpResponse>
  ValidateRegisterBrowserRequest(
      const enterprise_management::RegisterBrowserRequest&
          register_browser_request) override;

  constexpr base::flat_set<std::string> allowed_policy_types() override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_
