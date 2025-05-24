// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_message_pipe_server.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace named_mojo_ipc_server {

NamedMojoIpcServerBase::NamedMojoIpcServerBase(
    const EndpointOptions& options,
    base::RepeatingCallback<void*(const ConnectionInfo&)> impl_provider)
    : message_pipe_server_(
          options,
          impl_provider.Then(base::BindRepeating([](void* impl) {
            return NamedMojoMessagePipeServer::ValidationResult{
                .is_valid = impl != nullptr, .context = impl};
          })),
          // Safe to use Unretained(), since `message_pipe_server_` is destroyed
          // when NamedMojoIpcServerBase is being destroyed, which stops
          // callbacks from being called.
          base::BindRepeating(&NamedMojoIpcServerBase::OnMessagePipeReady,
                              base::Unretained(this))) {}

NamedMojoIpcServerBase::~NamedMojoIpcServerBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoIpcServerBase::StartServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_pipe_server_.StartServer();
}

void NamedMojoIpcServerBase::StopServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_pipe_server_.StopServer();
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

void NamedMojoIpcServerBase::OnIpcDisconnected() {
  if (disconnect_handler_) {
    disconnect_handler_.Run();
  }
  Close(current_receiver());
}

void NamedMojoIpcServerBase::OnMessagePipeReady(
    mojo::ScopedMessagePipeHandle message_pipe,
    std::unique_ptr<ConnectionInfo> connection_info,
    void* context,
    std::unique_ptr<mojo::IsolatedConnection> connection) {
  mojo::ReceiverId receiver_id = TrackMessagePipe(
      std::move(message_pipe), context, std::move(connection_info));
  if (connection) {
    active_connections_[receiver_id] = std::move(connection);
  }
}

}  // namespace named_mojo_ipc_server
