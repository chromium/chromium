// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/net/audio_socket_service.h"

#include <unistd.h>

#include <cstring>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/posix/unix_domain_socket.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/net/socket_util.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/socket/unix_domain_server_socket_posix.h"

namespace chromecast {
namespace media {

namespace {
constexpr int kListenBacklog = 10;
constexpr char kSocketMsg[] = "socket-handle";

void CloseSocket(int fd) {
  int rv = IGNORE_EINTR(close(fd));
  DPCHECK(rv == 0) << "Error closing socket";
}

}  // namespace

// static
std::unique_ptr<net::StreamSocket> AudioSocketService::Connect(
    const std::string& endpoint,
    int port) {
  return std::make_unique<net::UnixDomainClientSocket>(
      endpoint, true /* use_abstract_namespace */);
}

AudioSocketService::AudioSocketService(const std::string& endpoint,
                                       int port,
                                       int max_accept_loop,
                                       Delegate* delegate,
                                       bool use_socket_descriptor)
    : max_accept_loop_(max_accept_loop),
      use_socket_descriptor_(use_socket_descriptor),
      delegate_(delegate),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK_GT(max_accept_loop_, 0);
  DCHECK(delegate_);

  DCHECK(!endpoint.empty());
  LOG(INFO) << "Using endpoint " << endpoint;
  auto unix_socket = std::make_unique<net::UnixDomainServerSocket>(
      base::BindRepeating([](const net::UnixDomainServerSocket::Credentials&) {
        // Always accept the connection.
        return true;
      }),
      true /* use_abstract_namespace */);
  int result = unix_socket->BindAndListen(endpoint, kListenBacklog);
  listen_socket_ = std::move(unix_socket);

  if (result != net::OK) {
    LOG(ERROR) << "Listen failed: " << net::ErrorToString(result);
    listen_socket_.reset();
  }
}

int AudioSocketService::AcceptOne() {
  DCHECK(listen_socket_);

  if (use_socket_descriptor_) {
    return static_cast<net::UnixDomainServerSocket*>(listen_socket_.get())
        ->AcceptSocketDescriptor(
            &accepted_descriptor_,
            base::BindOnce(&AudioSocketService::OnAsyncAcceptComplete,
                           base::Unretained(this)));
  }
  return listen_socket_->Accept(
      &accepted_socket_,
      base::BindOnce(&AudioSocketService::OnAsyncAcceptComplete,
                     base::Unretained(this)));
}

void AudioSocketService::OnAcceptSuccess() {
  if (!use_socket_descriptor_) {
    delegate_->HandleAcceptedSocket(std::move(accepted_socket_));
    return;
  }

  if (accepted_descriptor_ == net::kInvalidSocket) {
    LOG(ERROR) << "Accepted socket descriptor is invalid.";
    return;
  }
  fd_watcher_controllers_.emplace(
      accepted_descriptor_,
      base::FileDescriptorWatcher::WatchReadable(
          accepted_descriptor_,
          base::BindRepeating(&AudioSocketService::ReceiveFdFromSocket,
                              base::Unretained(this), accepted_descriptor_)));
  accepted_descriptor_ = net::kInvalidSocket;
}

void AudioSocketService::ReceiveFdFromSocket(int socket_fd) {
  fd_watcher_controllers_.erase(socket_fd);

  char buffer[sizeof(kSocketMsg)];
  std::vector<base::ScopedFD> fds;
  ssize_t res =
      base::UnixDomainSocket::RecvMsg(socket_fd, buffer, sizeof(buffer), &fds);
  CloseSocket(socket_fd);
  if (res != sizeof(kSocketMsg)) {
    LOG(ERROR) << "Failed to receive message from the descriptor " << socket_fd;
    return;
  }
  if (memcmp(buffer, kSocketMsg, sizeof(kSocketMsg)) != 0) {
    LOG(ERROR) << "Received invalid message.";
    return;
  }
  if (fds.empty()) {
    LOG(ERROR) << "No socket descriptors received.";
    return;
  }
  for (auto& fd : fds) {
    delegate_->HandleAcceptedSocket(AdoptUnnamedSocketHandle(std::move(fd)));
  }
}

}  // namespace media
}  // namespace chromecast
