// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/grpc_unary_handler.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_service.castcore.pb.h"
#include "components/policy/test_support/remote_commands_state.h"

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

namespace fakedms {

void InitLogging(const std::optional<std::string>& log_path,
                 bool log_to_console,
                 int min_log_level);
void ParseFlags(const base::CommandLine& command_line,
                std::string& policy_blob_path,
                std::string& client_state_path,
                std::string& grpc_unix_socket_path,
                std::optional<std::string>& log_path,
                base::ScopedFD& startup_pipe,
                bool& log_to_console,
                int& min_log_level);

// TODO(b/293451778): Move this to its own file.
class RemoteCommandsWaitOperation;

class FakeDMServer : public policy::EmbeddedPolicyTestServer {
 public:
  FakeDMServer(const std::string& policy_blob_path,
               const std::string& client_state_path,
               const std::string& grpc_unix_socket_path,
               base::OnceClosure shutdown_cb = base::DoNothing());
  ~FakeDMServer() override;

  // Starts the EmbeddedPolicyTestServer on a different thread other than the
  // fake_dmserver_main thread, note that the EmbeddedPolicyTestServer will
  // shut down on its own when the destructor is called.
  // Starts the gRPC server on the same thread as the fake_dmserver_main thread.
  // Returns true if it's starts the servers successfully and false otherwise.
  bool StartFakeServer();

  // Writes the host and port of the EmbeddedPolicyTestServer to the given pipe
  // in a json format {"host": "localhost", "port": 1234}, it will return true
  // if it's able to write the URL to the pipe, and false otherwise.
  bool WriteURLToPipe(base::ScopedFD&& startup_pipe);

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

  // Parses regular and external policy from the JSON dict.
  bool ParsePolicies(const base::Value::Dict* dict);

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
  static std::optional<policy::ClientStorage::ClientInfo> GetClientFromValue(
      const base::Value& v);

  // Starts the gRPC server on the same thread as the fake_dmserver_main thread.
  void StartGrpcServer();

  // Shuts down the the gRPC server.
  void ShutdownGrpcServer(base::OnceClosure);

  // Resets the gRPC server and shuts down the fake_dmserver_main.
  void OnShutdownGrpcServerDone(base::OnceClosure);

  // Triggers Shutting down the fake_dmserver_main and the grpc server.
  void TriggerShutdown();

  // Writes the remote command result to the reactor, it will be triggered
  // from RemoteCommandsWaitOperation::OnRemoteCommandResultAvailable.
  void OnWaitRemoteCommandResultDone(
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
          Reactor*,
      int64_t,
      RemoteCommandsWaitOperation*,
      bool);
  // Writes the ack to the reactor, it will be triggered
  // from RemoteCommandsWaitOperation::OnRemoteCommandAcked.
  void OnWaitRemoteCommandAckDone(
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
          Reactor*,
      int64_t,
      RemoteCommandsWaitOperation*,
      bool);

  // Handles the RemoteCommandsService gRPC request SendRemoteCommand.
  void HandleSendRemoteCommand(
      remote_commands::SendRemoteCommandRequest request,
      remote_commands::RemoteCommandsServiceHandler::SendRemoteCommand::Reactor*
          reactor);

  // Handles the RemoteCommandsService gRPC request WaitRemoteCommandResult. If
  // the result isn't available withing 10 seconds, the grpc call will be
  // cancelled.
  void HandleWaitRemoteCommandResult(
      remote_commands::WaitRemoteCommandResultRequest request,
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
          Reactor* reactor);
  // Handles the RemoteCommandsService gRPC request WaitRemoteCommandAcked. If
  // the result isn't available withing 10 seconds, the grpc call will be
  // cancelled.
  void HandleWaitRemoteCommandAcked(
      remote_commands::WaitRemoteCommandAckedRequest request,
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
          Reactor* reactor);

  // Erase the wait operation from the waiters_ set.
  void EraseWaitOperation(RemoteCommandsWaitOperation*);

  const base::FilePath policy_blob_path_;
  const base::FilePath client_state_path_;
  std::string grpc_unix_socket_uri_;

  // Sequence checker for fake_dmserver main IO thread.
  SEQUENCE_CHECKER(fake_dmserver_main_sequence_checker_);
  // Sequence checker for embedded server's request handling thread, and all the
  // functions called by HandleRequest
  SEQUENCE_CHECKER(embedded_server_sequence_checker_);

  std::optional<cast::utils::GrpcServer> grpc_server_;
  // Callback to shut down the grpc server.
  base::OnceClosure shut_down_on_main_task_runner_;
  // Callback to reset the grpc server then shut down the fake_dmserver main.
  base::OnceClosure shut_down_server_;

  // Stores the wait operations to be erased later when the remote command
  // result is available.
  std::set<std::unique_ptr<RemoteCommandsWaitOperation>,
           base::UniquePtrComparator>
      waiters_;

  base::WeakPtrFactory<FakeDMServer> weak_ptr_factory_{this};
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
