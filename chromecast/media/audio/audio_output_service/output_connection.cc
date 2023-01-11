// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/output_connection.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "chromecast/net/socket_util.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

constexpr base::TimeDelta kReconnectDelay = base::Seconds(1);
constexpr int kMaxRetries = 3;

}  // namespace

OutputConnection::OutputConnection(
    mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker)
    : socket_broker_(std::move(pending_socket_broker)) {
  DCHECK(socket_broker_.is_connected());
}

OutputConnection::~OutputConnection() = default;

void OutputConnection::Connect() {
  DCHECK(!connecting_socket_);

  socket_broker_->GetSocketDescriptor(base::BindOnce(
      &OutputConnection::OnSocketDescriptor, weak_factory_.GetWeakPtr()));
}

void OutputConnection::OnSocketDescriptor(mojo::PlatformHandle handle) {
  if (!handle.is_valid_fd()) {
    LOG(ERROR) << "Received invalid socket descriptor.";
    HandleConnectResult(net::ERR_FAILED);
    return;
  }

  connecting_socket_ = AdoptUnnamedSocketHandle(handle.TakeFD());
  if (!connecting_socket_) {
    LOG(ERROR) << "Cannot adopt socket descriptor.";
    HandleConnectResult(net::ERR_FAILED);
    return;
  }
  DCHECK(connecting_socket_->IsConnected());
  HandleConnectResult(net::OK);
}

void OutputConnection::HandleConnectResult(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);

  if (result == net::OK) {
    log_connection_failure_ = true;
    retry_count_ = 0;
    auto socket = std::make_unique<OutputSocket>(std::move(connecting_socket_));
    OnConnected(std::move(socket));
    return;
  }

  base::TimeDelta delay = kReconnectDelay;
  if (log_connection_failure_) {
    LOG(ERROR) << "Failed to connect to audio output service: " << result;
    log_connection_failure_ = false;
    delay = base::TimeDelta();  // No reconnect delay on the first failure.
  }
  connecting_socket_.reset();

  if (++retry_count_ > kMaxRetries) {
    OnConnectionFailed();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OutputConnection::Connect, weak_factory_.GetWeakPtr()),
      delay);
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
