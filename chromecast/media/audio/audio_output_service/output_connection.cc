// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/output_connection.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/audio/audio_output_service/constants.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "chromecast/media/audio/net/audio_socket_service.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

constexpr base::TimeDelta kConnectTimeout = base::Seconds(1);

}  // namespace

OutputConnection::OutputConnection() = default;

OutputConnection::~OutputConnection() = default;

void OutputConnection::Connect() {
  DCHECK(!connecting_socket_);

  connecting_socket_ = AudioSocketService::Connect(
      kDefaultAudioOutputServiceUnixDomainSocketPath,
      kDefaultAudioOutputServiceTcpPort);
  int result = connecting_socket_->Connect(base::BindOnce(
      &OutputConnection::ConnectCallback, weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    ConnectCallback(result);
    return;
  }

  connection_timeout_.Start(FROM_HERE, kConnectTimeout, this,
                            &OutputConnection::ConnectTimeout);
}

void OutputConnection::ConnectCallback(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);

  connection_timeout_.Stop();
  if (result == net::OK) {
    LOG_IF(INFO, !log_timeout_) << "Now connected to audio output service.";
    log_connection_failure_ = true;
    log_timeout_ = true;
    auto socket = std::make_unique<OutputSocket>(std::move(connecting_socket_));
    OnConnected(std::move(socket));
    return;
  }

  base::TimeDelta delay = kConnectTimeout;
  if (log_connection_failure_) {
    LOG(ERROR) << "Failed to connect to audio output service: " << result;
    log_connection_failure_ = false;
    delay = base::TimeDelta();  // No reconnect delay on the first failure.
  }
  connecting_socket_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OutputConnection::Connect, weak_factory_.GetWeakPtr()),
      delay);
}

void OutputConnection::ConnectTimeout() {
  if (!connecting_socket_) {
    return;
  }

  if (log_timeout_) {
    LOG(ERROR) << "Timed out connecting to audio output service";
    log_timeout_ = false;
  }
  connecting_socket_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OutputConnection::Connect, weak_factory_.GetWeakPtr()));
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
