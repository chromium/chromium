// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_grpc_server.h"

#include <stdint.h>

#include <optional>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/grpc_unary_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/remote_commands_service.castcore.pb.h"
#include "components/policy/test_support/remote_commands_state.h"

namespace fakedms {

namespace {

namespace em = enterprise_management;

constexpr base::TimeDelta kRemoteCommandTimeoutSeconds = base::Seconds(10);
constexpr int64_t kDefaultServerStopTimeoutMs = 100;

remote_commands::WaitRemoteCommandResultResponse
BuildWaitRemoteCommandResultResponse(const em::RemoteCommandResult& result) {
  remote_commands::WaitRemoteCommandResultResponse resp;
  em::RemoteCommandResult* remote_command_result = resp.mutable_result();
  remote_command_result->set_result(result.result());
  remote_command_result->set_command_id(result.command_id());
  remote_command_result->set_timestamp(result.timestamp());
  remote_command_result->set_payload(result.payload());
  return resp;
}

enum class RemoteCommandsWaitType { kAcknowledged, kResultAvailable };

class RemoteCommandsWaitOperation
    : public policy::RemoteCommandsState::Observer {
 public:
  // Callback for a RemoteCommandsWaitOperation.
  // `wait_operation` will refer to the RemoteCommandsWaitOperation object that
  // invoked the callback. If `success` is true, the `RemoteCommandsWaitType`
  // has happened, otherwise the wait timed out.
  using RemoteCommandsWaitCallback =
      base::OnceCallback<void(RemoteCommandsWaitOperation* wait_operation,
                              bool success)>;

  RemoteCommandsWaitOperation(
      policy::RemoteCommandsState* remote_commands_state,
      RemoteCommandsWaitType wait_type,
      RemoteCommandsWaitOperation::RemoteCommandsWaitCallback wait_callback);

  ~RemoteCommandsWaitOperation() override;

  // policy::RemoteCommandsState::Observer:
  void OnRemoteCommandResultAvailable(int64_t command_id) override;
  void OnRemoteCommandAcked(int64_t command_id) override;

  void OnTimeout();

 private:
  const raw_ptr<policy::RemoteCommandsState> remote_commands_state_;
  const RemoteCommandsWaitType wait_type_;
  RemoteCommandsWaitCallback wait_callback_;
  base::ScopedObservation<policy::RemoteCommandsState,
                          policy::RemoteCommandsState::Observer>
      state_observation_{this};
  // Timer that fires to prevent indefinite wait if the remote command result
  // takes too long.
  base::OneShotTimer result_timeout_timer_;
  base::WeakPtrFactory<RemoteCommandsWaitOperation> weak_ptr_factory_{this};
};

RemoteCommandsWaitOperation::RemoteCommandsWaitOperation(
    policy::RemoteCommandsState* remote_commands_state,
    RemoteCommandsWaitType wait_type,
    RemoteCommandsWaitOperation::RemoteCommandsWaitCallback wait_callback)
    : remote_commands_state_(remote_commands_state),
      wait_type_(wait_type),
      wait_callback_(std::move(wait_callback)) {
  state_observation_.Observe(remote_commands_state);
  // Start a timer for 10 seconds to wait for the remote command result.
  result_timeout_timer_.Start(
      FROM_HERE, kRemoteCommandTimeoutSeconds,
      base::BindOnce(&RemoteCommandsWaitOperation::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

RemoteCommandsWaitOperation::~RemoteCommandsWaitOperation() = default;

void RemoteCommandsWaitOperation::OnRemoteCommandResultAvailable(
    int64_t command_id) {
  if (wait_type_ != RemoteCommandsWaitType::kResultAvailable) {
    return;
  }
  const bool result_available =
      remote_commands_state_->IsRemoteCommandResultAvailable(command_id);
  // The result must be available now.
  CHECK(result_available);
  // Invoke the wait callback OnWaitRemoteCommandResultDone to write the result
  // to the reactor.
  std::move(wait_callback_).Run(this, result_available);
}

void RemoteCommandsWaitOperation::OnRemoteCommandAcked(int64_t command_id) {
  if (wait_type_ != RemoteCommandsWaitType::kAcknowledged) {
    return;
  }
  const bool command_acked =
      remote_commands_state_->IsRemoteCommandAcked(command_id);
  // The command must be acknowledged now.
  CHECK(command_acked);
  // Invoke the wait callback OnWaitRemoteCommandAckDone to write the ack to the
  // reactor.
  std::move(wait_callback_).Run(this, command_acked);
}

void RemoteCommandsWaitOperation::OnTimeout() {
  std::move(wait_callback_).Run(this, false);
}

class RemoteCommandsGrpcServerImpl : public RemoteCommandsGrpcServer {
 public:
  RemoteCommandsGrpcServerImpl(
      const std::string& unix_socket_uri,
      policy::RemoteCommandsState* remote_commands_state);
  ~RemoteCommandsGrpcServerImpl() override;

  // RemoteCommandsGrpcServer:
  void Stop(base::OnceClosure server_stopped_callback) override;

 private:
  policy::RemoteCommandsState* remote_commands_state() {
    return remote_commands_state_;
  }

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

  const raw_ptr<policy::RemoteCommandsState> remote_commands_state_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<cast::utils::GrpcServer> grpc_server_;

  // Stores the wait operations to be erased later when the remote command
  // result is available.
  std::set<std::unique_ptr<RemoteCommandsWaitOperation>,
           base::UniquePtrComparator>
      waiters_;

  base::WeakPtrFactory<RemoteCommandsGrpcServerImpl> weak_ptr_factory_{this};
};

RemoteCommandsGrpcServerImpl::RemoteCommandsGrpcServerImpl(
    const std::string& unix_socket_uri,
    policy::RemoteCommandsState* remote_commands_state)
    : remote_commands_state_(remote_commands_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Starting the gRPC server on endpoint " << unix_socket_uri;
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::SendRemoteCommand>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(
              &RemoteCommandsGrpcServerImpl::HandleSendRemoteCommand,
              weak_ptr_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(
              &RemoteCommandsGrpcServerImpl::HandleWaitRemoteCommandResult,
              weak_ptr_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(
              &RemoteCommandsGrpcServerImpl::HandleWaitRemoteCommandAcked,
              weak_ptr_factory_.GetWeakPtr())));
  auto status = grpc_server_->Start(unix_socket_uri);
  // Browser runtime must crash if the runtime service failed to start to avoid
  // the process to dangle without any proper connection to the Cast Core.
  CHECK(status.ok()) << "Failed to start DM gRPC server: status="
                     << status.error_message();
}

RemoteCommandsGrpcServerImpl::~RemoteCommandsGrpcServerImpl() = default;

void RemoteCommandsGrpcServerImpl::HandleSendRemoteCommand(
    remote_commands::SendRemoteCommandRequest request,
    remote_commands::RemoteCommandsServiceHandler::SendRemoteCommand::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Processing SendRemoteCommand grpc request.";
  int64_t command_id = remote_commands_state()->AddPendingRemoteCommand(
      request.remote_command());
  remote_commands::SendRemoteCommandResponse resp;
  resp.set_command_id(command_id);
  reactor->Write(std::move(resp));
}

void RemoteCommandsGrpcServerImpl::OnWaitRemoteCommandResultDone(
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
        Reactor* reactor,
    int64_t command_id,
    RemoteCommandsWaitOperation* wait_operation,
    bool wait_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = waiters_.find(wait_operation);
  CHECK(it != waiters_.end());
  waiters_.erase(it);

  if (!wait_success) {
    reactor->Write(grpc::Status(
        grpc::StatusCode::CANCELLED,
        "Timeout waiting for remote command result took more than 10 seconds"));
    return;
  }
  em::RemoteCommandResult result;
  bool result_available =
      remote_commands_state()->GetRemoteCommandResult(command_id, &result);
  CHECK(result_available);
  auto resp = BuildWaitRemoteCommandResultResponse(result);
  reactor->Write(std::move(resp));
}

void RemoteCommandsGrpcServerImpl::HandleWaitRemoteCommandResult(
    remote_commands::WaitRemoteCommandResultRequest request,
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
        Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Processing WaitRemoteCommandResult grpc request.";
  int64_t command_id = request.command_id();
  em::RemoteCommandResult result;
  bool result_available =
      remote_commands_state()->GetRemoteCommandResult(command_id, &result);
  if (!result_available) {
    LOG(INFO) << "Remote command result isn't available yet.";
    // Insert the wait operation into the set and bind the erase function to
    // erase it if the result is available.
    waiters_.insert(std::make_unique<RemoteCommandsWaitOperation>(
        remote_commands_state(), RemoteCommandsWaitType::kResultAvailable,
        base::BindOnce(
            &RemoteCommandsGrpcServerImpl::OnWaitRemoteCommandResultDone,
            weak_ptr_factory_.GetWeakPtr(), base::Unretained(reactor),
            command_id)));
    return;
  }
  LOG(INFO) << "Remote command result is available. Resolving the grpc call.";
  auto resp = BuildWaitRemoteCommandResultResponse(result);
  reactor->Write(std::move(resp));
}

void RemoteCommandsGrpcServerImpl::OnWaitRemoteCommandAckDone(
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
        Reactor* reactor,
    int64_t command_id,
    RemoteCommandsWaitOperation* wait_operation,
    bool wait_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = waiters_.find(wait_operation);
  CHECK(it != waiters_.end());
  waiters_.erase(it);

  if (!wait_success) {
    reactor->Write(grpc::Status(grpc::StatusCode::CANCELLED,
                                "Timeout waiting for remote command "
                                "acknowledgement took more than 10 seconds"));
    return;
  }
  bool command_acked =
      remote_commands_state()->IsRemoteCommandAcked(command_id);
  CHECK(command_acked);
  remote_commands::WaitRemoteCommandAckedResponse resp;
  reactor->Write(std::move(resp));
}

void RemoteCommandsGrpcServerImpl::HandleWaitRemoteCommandAcked(
    remote_commands::WaitRemoteCommandAckedRequest request,
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
        Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Processing WaitRemoteCommandAcked grpc request.";
  int64_t command_id = request.command_id();
  bool command_acked =
      remote_commands_state()->IsRemoteCommandAcked(command_id);
  if (!command_acked) {
    LOG(INFO) << "Remote command isn't acknowledged yet.";
    // Insert the wait operation into the set and bind the erase function to
    // erase it if the command is acknowledged.
    waiters_.insert(std::make_unique<RemoteCommandsWaitOperation>(
        remote_commands_state(), RemoteCommandsWaitType::kAcknowledged,
        base::BindOnce(
            &RemoteCommandsGrpcServerImpl::OnWaitRemoteCommandAckDone,
            weak_ptr_factory_.GetWeakPtr(), base::Unretained(reactor),
            command_id)));
    return;
  }
  LOG(INFO) << "Remote command is acknowledged. Resolving the grpc call.";
  remote_commands::WaitRemoteCommandAckedResponse resp;
  reactor->Write(std::move(resp));
}

void RemoteCommandsGrpcServerImpl::Stop(
    base::OnceClosure server_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(grpc_server_);
  grpc_server_->Stop(kDefaultServerStopTimeoutMs,
                     std::move(server_stopped_callback));
}

}  // namespace

// static
std::unique_ptr<RemoteCommandsGrpcServer> RemoteCommandsGrpcServer::Start(
    const std::string& unix_socket_uri,
    policy::RemoteCommandsState* remote_commands_state) {
  return std::make_unique<RemoteCommandsGrpcServerImpl>(unix_socket_uri,
                                                        remote_commands_state);
}

}  // namespace fakedms
