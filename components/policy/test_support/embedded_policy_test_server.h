// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "components/policy/proto/device_management_backend.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace policy {

class ClientStorage;
class PolicyStorage;
class RemoteCommandsState;

extern const char kFakeDeviceToken[];
extern const char kInvalidEnrollmentToken[];

// Runs a fake implementation of the cloud policy server on the local machine.
class EmbeddedPolicyTestServer {
 public:
  class RequestHandler {
   public:
    explicit RequestHandler(EmbeddedPolicyTestServer* parent);
    virtual ~RequestHandler();

    // Returns the value associated with the "request_type" query param handled
    // by this request handler.
    virtual std::string RequestType() = 0;

    // Returns a response if this request can be handled by this handler, or
    // nullptr otherwise.
    virtual std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
        const net::test_server::HttpRequest& request) = 0;

    const ClientStorage* client_storage() const {
      return parent_->client_storage();
    }
    ClientStorage* client_storage() { return parent_->client_storage(); }

    const PolicyStorage* policy_storage() const {
      return parent_->policy_storage();
    }
    PolicyStorage* policy_storage() { return parent_->policy_storage(); }

    RemoteCommandsState* remote_commands_state() {
      return parent_->remote_commands_state();
    }

   private:
    const raw_ptr<EmbeddedPolicyTestServer> parent_;
  };

  EmbeddedPolicyTestServer();
  EmbeddedPolicyTestServer(const EmbeddedPolicyTestServer&) = delete;
  EmbeddedPolicyTestServer& operator=(const EmbeddedPolicyTestServer&) = delete;
  virtual ~EmbeddedPolicyTestServer();

  // Initializes and waits until the server is ready to accept requests.
  virtual bool Start();

  ClientStorage* client_storage();

  PolicyStorage* policy_storage();

  RemoteCommandsState* remote_commands_state();

  // Returns the service URL.
  GURL GetServiceURL() const;

  // Public so it can be used by tests.
  void RegisterHandler(std::unique_ptr<EmbeddedPolicyTestServer::RequestHandler>
                           request_handler);

  // Configures requests of a given |request_type| to always fail with
  // |error_code|.
  void ConfigureRequestError(const std::string& request_type,
                             net::HttpStatusCode error_code);

  // Resets the server state.
  void ResetServerState();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Updates policy selected by |type| and optional |entity_id|. The
  // |raw_policy| is served via an external endpoint. This does not trigger
  // policy invalidation, hence test authors must manually trigger a policy
  // fetch.
  void UpdateExternalPolicy(const std::string& type,
                            const std::string& entity_id,
                            const std::string& raw_policy);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

 protected:
  // Default request handler.
  virtual std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  // Request handler for external policy data.
  std::unique_ptr<net::test_server::HttpResponse>
  HandleExternalPolicyDataRequest(const GURL& request);

  net::test_server::EmbeddedTestServer http_server_;
  std::map<std::string, std::unique_ptr<RequestHandler>> request_handlers_;

  // ServerState contains all the fields that represent the server state.
  struct ServerState;
  std::unique_ptr<ServerState> server_state_;

  // TODO(b/275564884): Combine the remote commands state with the server state.
  // Separate because fake_dm_server clears server_state_ on each handler call.
  std::unique_ptr<RemoteCommandsState> remote_commands_state_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_
