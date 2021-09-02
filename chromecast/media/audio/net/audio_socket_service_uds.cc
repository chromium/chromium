// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/net/audio_socket_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/socket/unix_domain_server_socket_posix.h"

namespace chromecast {
namespace media {

namespace {
constexpr int kListenBacklog = 10;
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
                                       Delegate* delegate)
    : max_accept_loop_(max_accept_loop),
      delegate_(delegate),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
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

}  // namespace media
}  // namespace chromecast
