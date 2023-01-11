// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_connection.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/net/audio_socket_service.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

constexpr base::TimeDelta kConnectTimeout = base::Seconds(1);

}  // namespace

std::unique_ptr<MixerSocket> CreateLocalMixerServiceConnection()
    __attribute__((__weak__));

MixerConnection::MixerConnection() : weak_factory_(this) {}

MixerConnection::~MixerConnection() = default;

void MixerConnection::Connect() {
  DCHECK(!connecting_socket_);
  if (CreateLocalMixerServiceConnection) {
    auto socket = CreateLocalMixerServiceConnection();
    if (socket) {
      OnConnected(std::move(socket));
      return;
    }
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string path =
      command_line->GetSwitchValueASCII(switches::kMixerServiceEndpoint);
  if (path.empty()) {
    path = kDefaultUnixDomainSocketPath;
  }
  int port = GetSwitchValueNonNegativeInt(switches::kMixerServicePort,
                                          kDefaultTcpPort);
  connecting_socket_ = AudioSocketService::Connect(path, port);

  int result = connecting_socket_->Connect(base::BindOnce(
      &MixerConnection::ConnectCallback, weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    ConnectCallback(result);
    return;
  }

  connection_timeout_.Start(FROM_HERE, kConnectTimeout, this,
                            &MixerConnection::ConnectTimeout);
}

void MixerConnection::ConnectCallback(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  connection_timeout_.Stop();
  if (result == net::OK) {
    LOG_IF(INFO, !log_timeout_) << "Now connected to mixer service";
    log_connection_failure_ = true;
    log_timeout_ = true;
    auto socket =
        std::make_unique<MixerSocketImpl>(std::move(connecting_socket_));
    OnConnected(std::move(socket));
    return;
  }

  base::TimeDelta delay = kConnectTimeout;
  if (log_connection_failure_) {
    LOG(ERROR) << "Failed to connect to mixer service: " << result;
    log_connection_failure_ = false;
    delay = base::TimeDelta();  // No reconnect delay on the first failure.
  }
  connecting_socket_.reset();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MixerConnection::Connect, weak_factory_.GetWeakPtr()),
      delay);
}

void MixerConnection::ConnectTimeout() {
  if (!connecting_socket_) {
    return;
  }

  if (log_timeout_) {
    LOG(ERROR) << "Timed out connecting to mixer service";
    log_timeout_ = false;
  }
  connecting_socket_.reset();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MixerConnection::Connect, weak_factory_.GetWeakPtr()));
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
