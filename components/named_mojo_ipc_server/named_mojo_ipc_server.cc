// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#if BUILDFLAG(IS_WIN)
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
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::ScopedMessagePipeHandle message_pipe,
    base::ProcessId peer_pid) {
  if (server_)
    server_->OnServerEndpointConnected(std::move(connection),
                                       std::move(message_pipe), peer_pid);
}

void NamedMojoIpcServerBase::DelegateProxy::OnServerEndpointConnectionFailed() {
  if (server_)
    server_->OnServerEndpointConnectionFailed();
}

NamedMojoIpcServerBase::NamedMojoIpcServerBase(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    IsTrustedMojoEndpointCallback is_trusted_endpoint_callback)
    : server_name_(server_name),
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
      io_sequence_);
  server_started_ = true;
  SendInvitation();
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

void NamedMojoIpcServerBase::SendInvitation() {
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
      .Then(on_invitation_sent_callback_for_testing_);
}

void NamedMojoIpcServerBase::OnServerEndpointConnected(
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::ScopedMessagePipeHandle message_pipe,
    base::ProcessId peer_pid) {
  if (is_trusted_endpoint_callback_.Run(peer_pid)) {
    auto receiver_id = TrackMessagePipe(std::move(message_pipe), peer_pid);
    active_connections_[receiver_id] = std::move(connection);
  } else {
    LOG(ERROR) << "Process " << peer_pid
               << " is not a trusted mojo endpoint. Connection refused.";
  }

  SendInvitation();
}

void NamedMojoIpcServerBase::OnServerEndpointConnectionFailed() {
  resent_invitation_on_error_timer_.Start(
      FROM_HERE, kResentInvitationOnErrorDelay, this,
      &NamedMojoIpcServerBase::SendInvitation);
}

}  // namespace named_mojo_ipc_server
