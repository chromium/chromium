// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace named_mojo_ipc_server {

namespace {

// Delay to throttle resending invitations when there is a recurring error.
// TODO(yuweih): Implement backoff.
base::TimeDelta kResentInvitationOnErrorDelay = base::Seconds(5);

mojo::PlatformChannelServerEndpoint CreateServerEndpointOnIoSequence(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;

#if BUILDFLAG(IS_WIN)
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to get user SID string.";
    return mojo::PlatformChannelServerEndpoint();
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());
#endif  // BUILDFLAG(IS_WIN)

  mojo::NamedPlatformChannel channel(options);
  return channel.TakeServerEndpoint();
}

}  // namespace

NamedMojoIpcServerBase::DelegateProxy::DelegateProxy(
    base::WeakPtr<NamedMojoIpcServerBase> server)
    : server_(server) {}

NamedMojoIpcServerBase::DelegateProxy::~DelegateProxy() = default;

void NamedMojoIpcServerBase::DelegateProxy::OnServerEndpointConnected(
    mojo::PlatformChannelEndpoint endpoint,
    base::ProcessId peer_pid) {
  if (server_)
    server_->OnServerEndpointConnected(std::move(endpoint), peer_pid);
}

void NamedMojoIpcServerBase::DelegateProxy::OnServerEndpointConnectionFailed() {
  if (server_)
    server_->OnServerEndpointConnectionFailed();
}

NamedMojoIpcServerBase::NamedMojoIpcServerBase(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    absl::optional<uint64_t> message_pipe_id,
    IsTrustedMojoEndpointCallback is_trusted_endpoint_callback)
    : server_name_(server_name),
      message_pipe_id_(message_pipe_id),
      is_trusted_endpoint_callback_(std::move(is_trusted_endpoint_callback)) {
  io_sequence_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

NamedMojoIpcServerBase::~NamedMojoIpcServerBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoIpcServerBase::StartServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (server_started_) {
    return;
  }

  endpoint_connector_ = NamedMojoServerEndpointConnector::Create(
      base::SequenceBound<DelegateProxy>(
          base::SequencedTaskRunner::GetCurrentDefault(),
          weak_factory_.GetWeakPtr()),
      io_sequence_, server_name_);
  server_started_ = true;
  CreateServerEndpoint();
}

void NamedMojoIpcServerBase::StopServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!server_started_) {
    return;
  }
  server_started_ = false;
  endpoint_connector_.Reset();
  UntrackAllMessagePipes();
  active_connections_.clear();
}

void NamedMojoIpcServerBase::Close(mojo::ReceiverId id) {
  UntrackMessagePipe(id);
  auto it = active_connections_.find(id);
  if (it != active_connections_.end()) {
    active_connections_.erase(it);
  }
}

void NamedMojoIpcServerBase::CreateServerEndpoint() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateServerEndpointOnIoSequence, server_name_),
      base::BindOnce(&NamedMojoIpcServerBase::OnServerEndpointCreated,
                     weak_factory_.GetWeakPtr()));
}

void NamedMojoIpcServerBase::OnIpcDisconnected() {
  if (disconnect_handler_) {
    disconnect_handler_.Run();
  }
  Close(current_receiver());
}

void NamedMojoIpcServerBase::OnServerEndpointCreated(
    mojo::PlatformChannelServerEndpoint endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!server_started_) {
    // A server endpoint might be created on |io_sequence_| after StopServer()
    // is called, which should be ignored.
    return;
  }

  if (!endpoint.is_valid()) {
    OnServerEndpointConnectionFailed();
    return;
  }

  endpoint_connector_.AsyncCall(&NamedMojoServerEndpointConnector::Connect)
      .WithArgs(std::move(endpoint))
      .Then(on_server_endpoint_created_callback_for_testing_);
}

void NamedMojoIpcServerBase::OnServerEndpointConnected(
    mojo::PlatformChannelEndpoint endpoint,
    base::ProcessId peer_pid) {
  PassAndTrackMessagePipe(std::move(endpoint), peer_pid);
  CreateServerEndpoint();
}

void NamedMojoIpcServerBase::OnServerEndpointConnectionFailed() {
  resend_invitation_on_error_timer_.Start(
      FROM_HERE, kResentInvitationOnErrorDelay, this,
      &NamedMojoIpcServerBase::CreateServerEndpoint);
}

void NamedMojoIpcServerBase::PassAndTrackMessagePipe(
    mojo::PlatformChannelEndpoint endpoint,
    base::ProcessId peer_pid) {
  if (!is_trusted_endpoint_callback_.Run(peer_pid)) {
    LOG(ERROR) << "Process " << peer_pid
               << " is not a trusted mojo endpoint. Connection refused.";
    return;
  }

  if (!message_pipe_id_.has_value()) {
    // Create isolated connection.
    auto connection = std::make_unique<mojo::IsolatedConnection>();
    mojo::ScopedMessagePipeHandle message_pipe =
        connection->Connect(std::move(endpoint));
    mojo::ReceiverId receiver_id =
        TrackMessagePipe(std::move(message_pipe), peer_pid);
    active_connections_[receiver_id] = std::move(connection);
    return;
  }

  // Create non-isolated connection.
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle message_pipe =
      invitation.AttachMessagePipe(*message_pipe_id_);
#if BUILDFLAG(IS_WIN)
  // Open process with minimum permissions since the client process might have
  // restricted its access with DACL.
  base::Process peer_process =
      base::Process::OpenWithAccess(peer_pid, PROCESS_DUP_HANDLE);
// Windows opens the process with a system call so we use PLOG to extract more
// info. Other OSes (i.e. POSIX) don't do that.
#define INVALID_PROCESS_LOG PLOG
#else
  base::Process peer_process = base::Process::Open(peer_pid);
#define INVALID_PROCESS_LOG LOG
#endif
  if (!peer_process.IsValid()) {
    INVALID_PROCESS_LOG(ERROR) << "Failed to open peer process";
    return;
  }
#undef INVALID_PROCESS_LOG
  mojo::OutgoingInvitation::Send(std::move(invitation), peer_process.Handle(),
                                 std::move(endpoint));
  TrackMessagePipe(std::move(message_pipe), peer_pid);
}

}  // namespace named_mojo_ipc_server
