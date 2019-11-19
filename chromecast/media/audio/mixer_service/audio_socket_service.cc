// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/audio_socket_service.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/media/audio/audio_buildflags.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

#if BUILDFLAG(USE_UNIX_SOCKETS)
#include "net/socket/unix_domain_server_socket_posix.h"
#else
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#endif

namespace chromecast {
namespace media {

namespace {

constexpr int kListenBacklog = 10;

}  // namespace

AudioSocketService::AudioSocketService(const std::string& endpoint,
                                       int port,
                                       int max_accept_loop,
                                       Delegate* delegate)
    : max_accept_loop_(max_accept_loop),
      delegate_(delegate),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK_GT(max_accept_loop_, 0);
  DCHECK(delegate_);

#if BUILDFLAG(USE_UNIX_SOCKETS)
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
#else
  DCHECK_GE(port, 0);
  LOG(INFO) << "Using port " << port;
  listen_socket_ = std::make_unique<net::TCPServerSocket>(nullptr /* net_log */,
                                                          net::NetLogSource());
  int result = listen_socket_->Listen(
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), port), kListenBacklog);
#endif  // BUILDFLAG(USE_UNIX_SOCKETS)

  if (result != net::OK) {
    LOG(ERROR) << "Listen failed: " << net::ErrorToString(result);
    listen_socket_.reset();
  }
}

AudioSocketService::~AudioSocketService() = default;

void AudioSocketService::Accept() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!listen_socket_) {
    return;
  }

  for (int i = 0; i < max_accept_loop_; ++i) {
    int result = listen_socket_->Accept(
        &accepted_socket_, base::BindRepeating(&AudioSocketService::OnAccept,
                                               base::Unretained(this)));
    // If the result is ERR_IO_PENDING, OnAccept() will eventually be
    // called; it will resume the accept loop.
    if (result == net::ERR_IO_PENDING || !HandleAcceptResult(result)) {
      return;
    }
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&AudioSocketService::Accept,
                                                   base::Unretained(this)));
}

void AudioSocketService::OnAccept(int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (HandleAcceptResult(result)) {
    Accept();
  }
}

bool AudioSocketService::HandleAcceptResult(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Accept failed: " << net::ErrorToString(result);
    return false;
  }
  delegate_->HandleAcceptedSocket(std::move(accepted_socket_));
  return true;
}

}  // namespace media
}  // namespace chromecast
