// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector_linux.h"

#include <sys/socket.h>

#include <memory>

#include "base/check.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace named_mojo_ipc_server {

NamedMojoServerEndpointConnectorLinux::NamedMojoServerEndpointConnectorLinux(
    base::SequenceBound<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

NamedMojoServerEndpointConnectorLinux::
    ~NamedMojoServerEndpointConnectorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoServerEndpointConnectorLinux::Connect(
    mojo::PlatformChannelServerEndpoint server_endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(server_endpoint.is_valid());
  DCHECK(!pending_server_endpoint_.is_valid());

  pending_server_endpoint_ = std::move(server_endpoint);
  read_watcher_controller_ = base::FileDescriptorWatcher::WatchReadable(
      pending_server_endpoint_.platform_handle().GetFD().get(),
      base::BindRepeating(
          &NamedMojoServerEndpointConnectorLinux::OnFileCanReadWithoutBlocking,
          weak_factory_.GetWeakPtr()));
}

void NamedMojoServerEndpointConnectorLinux::OnFileCanReadWithoutBlocking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int fd = pending_server_endpoint_.platform_handle().GetFD().get();

  base::ScopedFD socket;
  bool success = mojo::AcceptSocketConnection(fd, &socket);
  read_watcher_controller_.reset();
  pending_server_endpoint_.reset();
  if (!success) {
    LOG(ERROR) << "AcceptSocketConnection failed.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }
  if (!socket.is_valid()) {
    LOG(ERROR) << "Socket is invalid.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }

  ucred unix_peer_identity;
  socklen_t len = sizeof(unix_peer_identity);
  if (getsockopt(socket.get(), SOL_SOCKET, SO_PEERCRED, &unix_peer_identity,
                 &len) != 0) {
    PLOG(ERROR) << "getsockopt failed.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }

  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(socket)));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }
  auto connection = std::make_unique<mojo::IsolatedConnection>();
  auto message_pipe = connection->Connect(std::move(endpoint));
  delegate_.AsyncCall(&Delegate::OnServerEndpointConnected)
      .WithArgs(std::move(connection), std::move(message_pipe),
                unix_peer_identity.pid);
}

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    base::SequenceBound<Delegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> io_sequence) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorLinux>(
      io_sequence, std::move(delegate));
}

}  // namespace named_mojo_ipc_server
