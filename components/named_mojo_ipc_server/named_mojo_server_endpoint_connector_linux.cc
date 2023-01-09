// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace named_mojo_ipc_server {
namespace {

class NamedMojoServerEndpointConnectorLinux final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorLinux(
      const EndpointOptions& options,
      base::SequenceBound<Delegate> delegate);
  NamedMojoServerEndpointConnectorLinux(
      const NamedMojoServerEndpointConnectorLinux&) = delete;
  NamedMojoServerEndpointConnectorLinux& operator=(
      const NamedMojoServerEndpointConnectorLinux&) = delete;
  ~NamedMojoServerEndpointConnectorLinux() override;

 private:
  void OnSocketReady();

  // Overrides for NamedMojoServerEndpointConnector.
  bool TryStart() override;

  // Note that |server_endpoint_| must outlive |read_watcher_controller_|;
  // otherwise a bad file descriptor error will occur at destruction.
  mojo::PlatformChannelServerEndpoint server_endpoint_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      read_watcher_controller_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<NamedMojoServerEndpointConnectorLinux> weak_factory_{
      this};
};

NamedMojoServerEndpointConnectorLinux::NamedMojoServerEndpointConnectorLinux(
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate)
    : NamedMojoServerEndpointConnector(options, std::move(delegate)) {}

NamedMojoServerEndpointConnectorLinux::
    ~NamedMojoServerEndpointConnectorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoServerEndpointConnectorLinux::OnSocketReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(server_endpoint_.is_valid());

  int fd = server_endpoint_.platform_handle().GetFD().get();

  base::ScopedFD connection_fd;
  bool success = mojo::AcceptSocketConnection(fd, &connection_fd);
  if (!success) {
    LOG(ERROR) << "AcceptSocketConnection failed.";
    return;
  }
  if (!connection_fd.is_valid()) {
    LOG(ERROR) << "Socket is invalid.";
    return;
  }

  auto info = std::make_unique<ConnectionInfo>();
  socklen_t len = sizeof(info->credentials);
  if (getsockopt(connection_fd.get(), SOL_SOCKET, SO_PEERCRED,
                 &info->credentials, &len) != 0) {
    PLOG(ERROR) << "getsockopt failed.";
    return;
  }
  info->pid = info->credentials.pid;

  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(connection_fd)));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    return;
  }
  delegate_.AsyncCall(&Delegate::OnClientConnected)
      .WithArgs(std::move(endpoint), std::move(info));
}

bool NamedMojoServerEndpointConnectorLinux::TryStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PlatformChannelServerEndpoint server_endpoint =
      mojo::NamedPlatformChannel({options_.server_name}).TakeServerEndpoint();
  if (!server_endpoint.is_valid()) {
    return false;
  }

  server_endpoint_ = std::move(server_endpoint);
  read_watcher_controller_ = base::FileDescriptorWatcher::WatchReadable(
      server_endpoint_.platform_handle().GetFD().get(),
      base::BindRepeating(&NamedMojoServerEndpointConnectorLinux::OnSocketReady,
                          weak_factory_.GetWeakPtr()));
  delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);
  return true;
}

}  // namespace

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorLinux>(
      io_sequence, options, std::move(delegate));
}

}  // namespace named_mojo_ipc_server
