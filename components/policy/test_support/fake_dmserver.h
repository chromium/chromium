// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/grpc_unary_handler.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_invalidator.h"
#include "components/policy/test_support/remote_commands_service.castcore.pb.h"
#include "components/policy/test_support/request_handler_for_admin.h"
#include "net/test/embedded_test_server/http_request.h"

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

enum class ParseFlagsResult {
  kSuccess,
  kHelp,
  kError,
};

void InitLogging(const std::optional<std::string>& log_path,
                 bool log_to_console,
                 int min_log_level);
void PrintHelp();

struct FakeServerConfig {
  std::string policy_blob_path;
  std::string client_state_path;
  std::string grpc_unix_socket_uri;
  std::optional<std::string> log_path;
  base::ScopedFD startup_pipe;
  bool log_to_console = false;
  int min_log_level = 0;
  int port = 0;
  // std::nullopt disables the FakeFcmServer.
  std::optional<int> fcm_port;
};

ParseFlagsResult ParseFlags(const base::CommandLine& command_line,
                            FakeServerConfig& config);

// TODO(b/293451778): Move this to its own file.
class RemoteCommandsWaitOperation;

// Cloud policy test server for device management, FCM push invalidations, and
// gRPC remote commands.
//
// Threading model:
// - The main sequence owns server lifecycle, request handling, gRPC requests,
//   FakeFcmServer, and PolicyInvalidator. Destruction stops the HTTP server
//   first so no HTTP tasks can be posted during teardown.
// - EmbeddedTestServer invokes HandleRequest() on its internal IO thread, which
//   returns an async HttpResponse wrapper that hops to `main_task_runner_` and
//   replies back on the IO thread.
// - `RemoteCommandsState` still uses its own lock because it is shared with
//   EmbeddedPolicyTestServer browser tests.
class FakeDMServer : public policy::EmbeddedPolicyTestServer,
                     public policy::RequestHandlerForAdmin::Delegate {
 public:
  FakeDMServer(const std::string& policy_blob_path,
               const std::string& client_state_path,
               const std::string& grpc_unix_socket_path,
               scoped_refptr<base::SequencedTaskRunner> debounce_task_runner,
               base::OnceClosure shutdown_cb = base::DoNothing());
  ~FakeDMServer() override;

  // Starts the EmbeddedPolicyTestServer on a different thread other than the
  // fake_dmserver_main thread, note that the EmbeddedPolicyTestServer will
  // shut down on its own when the destructor is called.
  // Starts the gRPC server on the same thread as the fake_dmserver_main thread.
  // Starts the FakeFcmServer; an FCM port conflict logs a warning and starts
  // without FCM. Returns true if the HTTP server starts, and false otherwise.
  // If `port` or `fcm_port` is 0, the server listens on a random port. If
  // `fcm_port` is std::nullopt, the FakeFcmServer does not start.
  bool StartFakeServer(int port = 0, std::optional<int> fcm_port = 0);

  // Returns the FakeFcmServer.
  FakeFcmServer* fcm_server() { return policy_invalidator_.fcm_server(); }

  // Writes server connection info as JSON to `startup_pipe` (see
  // WriteURLToPipe() for fields). Returns true on success.
  bool WriteURLToPipe(base::ScopedFD&& startup_pipe);

  // Overrides the EmbeddedPolicyTestServer request handler.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  class DeferredHttpResponse;

  struct MainThreadResponse {
    MainThreadResponse();
    MainThreadResponse(MainThreadResponse&&);
    MainThreadResponse& operator=(MainThreadResponse&&);
    ~MainThreadResponse();

    std::unique_ptr<net::test_server::HttpResponse> response;
    base::OnceClosure after_send;
  };

  static MainThreadResponse RunRequestOnMainThread(
      base::WeakPtr<FakeDMServer> server,
      net::test_server::HttpRequest request);

  // policy::RequestHandlerForAdmin::Delegate:
  std::optional<bool> TriggerPolicyInvalidation() override;
  std::string GetPoliciesJson() const override;
  bool UpdatePoliciesJson(const std::string& json_content) override;

  // Handles `request` on the main sequence. `after_send` is optionally set to
  // a main-sequence closure that runs after the HTTP response is sent.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequestOnMainThread(
      const net::test_server::HttpRequest& request,
      base::OnceClosure& after_send);

  // Resets the server state and reloads policies from `policy_blob_path_`.
  bool ReloadPoliciesFromFile();

  // Loads previously persisted binary reports from the data directory.
  void LoadPersistedReports(const base::FilePath& data_dir);

  // Loads previously persisted real-time reports from the reports directory.
  void LoadPersistedRealtimeReports(const base::FilePath& reports_dir);

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

  // Parses regular and external policies from the JSON dict. Policies in this
  // dict should be defined as plain JSON values, for example:
  //   {"user": {"AllowDinosaurEasterEgg": true}}
  bool ParsePoliciesJson(const base::DictValue* dict);

  // Parses regular and external policy from the JSON dict. Policies in this
  // dict should be defined as baked-in blobs generated by blob_generator.py,
  // for example:
  //   {"policies": [{"policy_type": "google/chrome/user",
  //                  "value": "<base64-encoded policy proto>"}]}
  bool ParsePolicyBlobs(const base::DictValue* dict);

  // Reads and sets the values in the policy blob file, it will return true if
  // the policy blob file doesn't exist yet or all the values are read
  // correctly, and false otherwise.
  bool ReadPolicyBlobFile();

  // Sets the values of the policy `blob` in the server state. Returns false if
  // a value is invalid.
  bool ParsePolicyBlob(const base::DictValue& blob);

  // Writes all the clients to the client state file, it will return true if
  // it's able to write the client storage to the state file, and false
  // otherwise.
  bool WriteClientStateFile();
  // Reads the client state file and registers the clients, it will return true
  // if the state file doesn't exist yet or all the values are read
  // correctly, and false otherwise.
  bool ReadClientStateFile();

  // Returns true if the key of the specific type is in the dictionary.
  static bool FindKey(const base::DictValue& dict,
                      const std::string& key,
                      base::Value::Type type);

  // Converts the client to Dictionary.
  static base::DictValue GetValueFromClient(
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

  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Sequence checker for fake_dmserver main IO thread.
  SEQUENCE_CHECKER(fake_dmserver_main_sequence_checker_);

  std::optional<cast::utils::GrpcServer> grpc_server_;
  // Callback to reset the grpc server then shut down the fake_dmserver main.
  base::OnceClosure shut_down_server_;

  // Stores the wait operations to be erased later when the remote command
  // result is available.
  std::set<std::unique_ptr<RemoteCommandsWaitOperation>,
           base::UniquePtrComparator>
      waiters_;

  PolicyInvalidator policy_invalidator_;

  base::WeakPtr<FakeDMServer> main_weak_ptr_;
  base::WeakPtrFactory<FakeDMServer> weak_ptr_factory_{this};
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FAKE_DMSERVER_H_
