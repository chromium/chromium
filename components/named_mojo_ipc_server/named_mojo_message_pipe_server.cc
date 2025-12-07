// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_message_pipe_server.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace named_mojo_ipc_server {

// Forwards callbacks from a NamedMojoServerEndpointConnector to a
// NamedMojoMessagePipeServer. This allows the server to create a SequenceBound
// interface to post callbacks from the IO sequence to the main sequence.
class NamedMojoMessagePipeServer::DelegateProxy final
    : public NamedMojoServerEndpointConnector::Delegate {
 public:
  explicit DelegateProxy(base::WeakPtr<NamedMojoMessagePipeServer> server)
      : server_(server) {}
  ~DelegateProxy() override = default;

  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         std::unique_ptr<ConnectionInfo> info) override {
    if (server_) {
      server_->OnClientConnected(std::move(endpoint), std::move(info));
    }
  }

  void OnServerEndpointCreated() override {
    if (server_) {
      server_->OnServerEndpointCreated();
    }
  }

 private:
  base::WeakPtr<NamedMojoMessagePipeServer> server_;
};

NamedMojoMessagePipeServer::NamedMojoMessagePipeServer(
    const EndpointOptions& options,
    Validator validator,
    OnMessagePipeReady on_message_pipe_ready)
    : options_(options),
      validator_(validator),
      on_message_pipe_ready_(on_message_pipe_ready) {
  CHECK(!options.server_name.empty());
  io_sequence_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

NamedMojoMessagePipeServer::~NamedMojoMessagePipeServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoMessagePipeServer::StartServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (server_started_) {
    return;
  }

  endpoint_connector_ = NamedMojoServerEndpointConnector::Create(
      io_sequence_, options_,
      base::SequenceBound<DelegateProxy>(
          base::SequencedTaskRunner::GetCurrentDefault(),
          weak_factory_.GetWeakPtr()));
  endpoint_connector_.AsyncCall(&NamedMojoServerEndpointConnector::Start);
  server_started_ = true;
}

void NamedMojoMessagePipeServer::StopServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!server_started_) {
    return;
  }
  server_started_ = false;
  endpoint_connector_.Reset();
}

void NamedMojoMessagePipeServer::OnClientConnected(
    mojo::PlatformChannelEndpoint endpoint,
    std::unique_ptr<ConnectionInfo> info) {
  base::ProcessId peer_pid = info->pid;
  ValidationResult result = validator_.Run(*info);
  if (!result.is_valid) {
    LOG(ERROR) << "Process " << peer_pid
               << " is not a trusted mojo endpoint. Connection refused.";
    return;
  }

  bool is_isolated =
      std::holds_alternative<std::monostate>(options_.message_pipe_id);

  base::Process peer_process;
  // A peer process is not needed to open a non-MojoIpcz isolated connection,
  // and in fact some callers don't have the right ACL to open the peer process
  // yet, so we only open the peer process if the connection is non-isolated, or
  // MojoIpcz is enabled.
  if (!is_isolated || mojo::core::IsMojoIpczEnabled()) {
#if BUILDFLAG(IS_WIN)
    // Open process with minimum permissions since the client process might have
    // restricted its access with DACL.
    peer_process = base::Process::OpenWithAccess(peer_pid, PROCESS_DUP_HANDLE);
// Windows opens the process with a system call so we use PLOG to extract more
// info. Other OSes (i.e. POSIX) don't do that.
#define INVALID_PROCESS_LOG PLOG
#else
    peer_process = base::Process::Open(peer_pid);
#define INVALID_PROCESS_LOG LOG
#endif
    if (!peer_process.IsValid()) {
      // With MojoIpcz, connections can be made without a process handle to the
      // client, as long as the client has a process handle to the server, so we
      // don't return here.
      INVALID_PROCESS_LOG(WARNING) << "Failed to open peer process";
    }
#undef INVALID_PROCESS_LOG
  }

  if (is_isolated) {
    // Create isolated connection.
    auto connection = std::make_unique<mojo::IsolatedConnection>();
    mojo::ScopedMessagePipeHandle message_pipe =
        connection->Connect(std::move(endpoint), std::move(peer_process));
    on_message_pipe_ready_.Run(std::move(message_pipe), std::move(info),
                               result.context, std::move(connection));
    return;
  }

  // Create non-isolated connection.
  mojo::OutgoingInvitation invitation;
  invitation.set_extra_flags(options_.extra_send_invitation_flags);
  mojo::ScopedMessagePipeHandle message_pipe =
      std::holds_alternative<uint64_t>(options_.message_pipe_id)
          ? invitation.AttachMessagePipe(
                std::get<uint64_t>(options_.message_pipe_id))
          : invitation.AttachMessagePipe(
                std::get<std::string>(options_.message_pipe_id));
  mojo::OutgoingInvitation::Send(std::move(invitation), peer_process.Handle(),
                                 std::move(endpoint));
  on_message_pipe_ready_.Run(std::move(message_pipe), std::move(info),
                             result.context, nullptr);
}

void NamedMojoMessagePipeServer::OnServerEndpointCreated() {
  if (on_server_endpoint_created_callback_for_testing_) {
    on_server_endpoint_created_callback_for_testing_.Run();
  }
}

}  // namespace named_mojo_ipc_server
