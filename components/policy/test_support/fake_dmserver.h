// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
A bare-bones test server for testing cloud policy support.

This implements a simple cloud policy test server that can be used to test
Chrome's device management service client. The policy information is read from
the file named policy.json in the server's data directory. It contains
policies for the device and user scope, and a list of managed users. The format
of the file is JSON.
The root dictionary contains a list under the key
"managed_users". It contains auth tokens for which the server will claim that
the user is managed. The token string "*" indicates that all users are claimed
to be managed.
The root dictionary also contains a list under the key "policies". It contains
all the policies to be set, each policy has 3 fields, "policy_type" is the type
or scope of the policy (user, device or publicaccount), "entity_id" is the
account id used for public account policies, "value" is the seralized proto
message of the policies value encoded in base64.
The root dictionary also contains a "policy_user" key which indicates the
current user.
All the fields are described in the device_management_backend.proto
(https://source.chromium.org/chromium/chromium/src/+/main:components/policy/proto/device_management_backend.proto;l=516?q=PolicyData)

Example:
{
  "policies" : [
    {
      "policy_type" : "google/chromeos/user",
      "value" : "base64 encoded proto message",
    },
    {
      "policy_type" : "google/chromeos/device",
      "value" : "base64 encoded proto message",
    },
    {
      "policy_type" : "google/chromeos/publicaccount",
      "entity_id" : "accountid@managedchrome.com",
      "value" : "base64 encoded proto message",
    }
  ],
  "external_policies" : [
    {
      "policy_type" : "google/chrome/extension",
      "entity_id" : "extension_id",
      "value" : "base64 encoded raw json value",
    }
  ],
  "managed_users" : [
    "secret123456"
  ],
  "policy_user" : "tast-user@managedchrome.com",
  "current_key_index": 0,
  "robot_api_auth_code": "code",
  "directory_api_id": "id",
  "request_errors": {
    "register": 500,
  }
  "device_affiliation_ids" : [
    "device_id"
  ],
  "user_affiliation_ids" : [
    "user_id"
  ],
  "allow_set_device_attributes" : false,
  "initial_enrollment_state": {
    "TEST_serial": {
      "initial_enrollment_mode": 2,
      "management_domain": "test-domain.com"
    }
  }
}
*/

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_

#include <string>

#include "base/command_line.h"
#include "base/values.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace fakedms {

void InitLogging(const absl::optional<std::string>& log_path,
                 bool log_to_console,
                 int min_log_level);
void ParseFlags(const base::CommandLine& command_line,
                std::string& policy_blob_path,
                std::string& client_state_path,
                absl::optional<std::string>& log_path,
                base::ScopedFD& startup_pipe,
                bool& log_to_console,
                int& min_log_level);

class FakeDMServer : public policy::EmbeddedPolicyTestServer {
 public:
  FakeDMServer(const std::string& policy_blob_path,
               const std::string& client_state_path,
               base::OnceClosure shutdown_cb = base::DoNothing());
  ~FakeDMServer() override;
  // Starts the FakeDMServer and EmbeddedPolicyTestServer, it will return true
  // if it's able to start the server successfully, and false otherwise.
  bool Start() override;

  // Writes the host and port of the EmbeddedPolicyTestServer to the given pipe
  // in a json format {"host": "localhost", "port": 1234}, it will return true
  // if it's able to write the URL to the pipe, and false otherwise.
  bool WriteURLToPipe(const base::ScopedFD& startup_pipe);

  // Overrides the EmbeddedPolicyTestServer request handler.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  // Sets the policy payload in the policy storage, it will return true if it's
  // able to set the policy and false otherwise.
  bool SetPolicyPayload(const std::string* policy_type,
                        const std::string* entity_id,
                        const std::string* serialized_proto);
  // Sets the external policy payload in the policy storage, it will return true
  // if it's able to set the policy and false otherwise.
  bool SetExternalPolicyPayload(const std::string* policy_type,
                                const std::string* entity_id,
                                const std::string* serialized_raw_policy);
  // Reads and sets the values in the policy blob file, it will return true if
  // the policy blob file doesn't exist yet or all the values are read
  // correctly, and false otherwise.
  bool ReadPolicyBlobFile();

  // Writes all the clients to the client state file, it will return true if
  // it's able to write the client storage to the state file, and false
  // otherwise.
  bool WriteClientStateFile();
  // Reads the client state file and registers the clients, it will return true
  // if the state file doesn't exist yet or all the values are read
  // correctly, and false otherwise.
  bool ReadClientStateFile();

  // Returns true if the key of the specific type is in the dictionary.
  static bool FindKey(const base::Value::Dict& dict,
                      const std::string& key,
                      base::Value::Type type);

  // Converts the client to Dictionary.
  static base::Value::Dict GetValueFromClient(
      const policy::ClientStorage::ClientInfo& c);
  // Converts the value to Client.
  static absl::optional<policy::ClientStorage::ClientInfo> GetClientFromValue(
      const base::Value& v);

  std::string policy_blob_path_, client_state_path_;
  base::OnceClosure shutdown_cb_;
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
