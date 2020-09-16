// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/audio_socket_service.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace chromecast {
namespace media {

namespace {
constexpr int kListenBacklog = 10;
}  // namespace

// static
std::unique_ptr<net::StreamSocket> AudioSocketService::Connect(
    const std::string& endpoint,
    int port) {
  net::IPEndPoint ip_endpoint(net::IPAddress::IPv4Localhost(), port);
  return std::make_unique<net::TCPClientSocket>(net::AddressList(ip_endpoint),
                                                nullptr, nullptr, nullptr,
                                                net::NetLogSource());
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

  DCHECK_GE(port, 0);
  LOG(INFO) << "Using port " << port;
  listen_socket_ = std::make_unique<net::TCPServerSocket>(nullptr /* net_log */,
                                                          net::NetLogSource());
  int result = listen_socket_->Listen(
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), port), kListenBacklog);

  if (result != net::OK) {
    LOG(ERROR) << "Listen failed: " << net::ErrorToString(result);
    listen_socket_.reset();
  }
}

}  // namespace media
}  // namespace chromecast
