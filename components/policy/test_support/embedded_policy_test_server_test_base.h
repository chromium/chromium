// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_TEST_BASE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace policy {

class ClientStorage;
class PolicyStorage;
class RemoteCommandsState;

class EmbeddedPolicyTestServerTestBase : public testing::Test {
 public:
  EmbeddedPolicyTestServerTestBase();
  EmbeddedPolicyTestServerTestBase(const EmbeddedPolicyTestServerTestBase&) =
      delete;
  EmbeddedPolicyTestServerTestBase& operator=(
      const EmbeddedPolicyTestServerTestBase&) = delete;
  ~EmbeddedPolicyTestServerTestBase() override;

  void SetUp() override;

  // Helper functions to set request components.
  void SetURL(const GURL& url);
  void SetMethod(const std::string& method);
  void SetAppType(const std::string& app_type);
  void SetDeviceIdParam(const std::string& device_id);
  void SetDeviceType(const std::string& device_type);
  void SetOAuthToken(const std::string& oauth_token);
  void SetRequestTypeParam(const std::string& request_type);
  void SetEnrollmentTokenHeader(const std::string& enrollment_token);
  void SetDeviceTokenHeader(const std::string& device_token);
  void SetGoogleLoginTokenHeader(const std::string& user_email);
  void SetPayload(const enterprise_management::DeviceManagementRequest&
                      device_management_request);

  // Makes a request to the test server and waits until a response is ready.
  void StartRequestAndWait();

  // Helper functions for accessing response data.
  int GetResponseCode() const;
  bool HasResponseBody() const;
  std::string GetResponseBody() const;
  enterprise_management::DeviceManagementResponse GetDeviceManagementResponse()
      const;

  EmbeddedPolicyTestServer* test_server() { return &test_server_; }

  ClientStorage* client_storage() { return test_server_.client_storage(); }

  PolicyStorage* policy_storage() { return test_server_.policy_storage(); }

  RemoteCommandsState* remote_commands_state() {
    return test_server_.remote_commands_state();
  }

 private:
  // Adds a query param to the |resource_request_|.
  void AddQueryParam(const std::string& key, const std::string& value);

  // Callback to be provided for the network request. Invokes |callback| when
  // done.
  void DownloadedToString(base::OnceClosure callback,
                          std::unique_ptr<std::string> response_body);

  base::test::TaskEnvironment task_environment_;
  EmbeddedPolicyTestServer test_server_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<network::ResourceRequest> resource_request_;
  std::string payload_;
  std::unique_ptr<std::string> response_body_;
  bool done_ = false;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_TEST_BASE_H_
