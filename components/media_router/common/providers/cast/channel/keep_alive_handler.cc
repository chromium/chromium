// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/keep_alive_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

KeepAliveHandler::KeepAliveHandler(CastSocket* socket,
                                   scoped_refptr<Logger> logger,
                                   base::TimeDelta ping_interval,
                                   base::TimeDelta liveness_timeout,
                                   OnErrorCallback on_error_cb)
    : started_(false),
      socket_(socket),
      logger_(logger),
      liveness_timeout_(liveness_timeout),
      ping_interval_(ping_interval),
      ping_message_(CreateKeepAlivePingMessage()),
      pong_message_(CreateKeepAlivePongMessage()),
      on_error_cb_(std::move(on_error_cb)) {
  DCHECK(ping_interval_ < liveness_timeout_);
  DCHECK(socket_);
  DCHECK(!on_error_cb_.is_null());
}

KeepAliveHandler::~KeepAliveHandler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void KeepAliveHandler::SetTimersForTest(
    std::unique_ptr<base::RetainingOneShotTimer> injected_ping_timer,
    std::unique_ptr<base::RetainingOneShotTimer> injected_liveness_timer) {
  ping_timer_ = std::move(injected_ping_timer);
  liveness_timer_ = std::move(injected_liveness_timer);
}

void KeepAliveHandler::Start() {
  DCHECK(!started_);

  DVLOG(1) << "Starting keep-alive timers.";
  DVLOG(1) << "Ping interval: " << ping_interval_;
  DVLOG(1) << "Liveness timeout: " << liveness_timeout_;

  // Use injected mock timers, if provided.
  if (!ping_timer_) {
    ping_timer_ = std::make_unique<base::RetainingOneShotTimer>();
  }
  if (!liveness_timer_) {
    liveness_timer_ = std::make_unique<base::RetainingOneShotTimer>();
  }

  ping_timer_->Start(
      FROM_HERE, ping_interval_,
      base::BindRepeating(&KeepAliveHandler::SendKeepAliveMessage,
                          base::Unretained(this), ping_message_,
                          CastMessageType::kPing));
  liveness_timer_->Start(FROM_HERE, liveness_timeout_,
                         base::BindRepeating(&KeepAliveHandler::LivenessTimeout,
                                             base::Unretained(this)));

  started_ = true;
}

bool KeepAliveHandler::HandleMessage(const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << "KeepAlive::OnMessage : " << message.payload_utf8();

  if (started_) {
    ResetTimers();
  }

  // Keep-alive messages are intercepted and handled by KeepAliveHandler
  // here. All other messages are passed through to |inner_delegate_|.
  // Keep-alive messages are assumed to be in the form { "type": "PING|PONG" }.
  if (message.namespace_() == kHeartbeatNamespace) {
    static const char* ping_message_type = ToString(CastMessageType::kPing);
    if (message.payload_utf8().find(ping_message_type) != std::string::npos) {
      DVLOG(2) << "Received PING.";
      if (started_)
        SendKeepAliveMessage(pong_message_, CastMessageType::kPong);
    } else {
      DCHECK_NE(std::string::npos,
                message.payload_utf8().find(ToString(CastMessageType::kPong)));
      DVLOG(2) << "Received PONG.";
    }
    return true;
  }
  return false;
}

void KeepAliveHandler::ResetTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(started_);
  ping_timer_->Reset();
  liveness_timer_->Reset();
}

void KeepAliveHandler::SendKeepAliveMessage(const CastMessage& message,
                                            CastMessageType message_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << "Sending " << ToString(message_type);

  socket_->transport()->SendMessage(
      message, base::BindOnce(&KeepAliveHandler::SendKeepAliveMessageComplete,
                              weak_factory_.GetWeakPtr(), message_type));
}

void KeepAliveHandler::SendKeepAliveMessageComplete(
    CastMessageType message_type,
    int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << "Sending " << ToString(message_type) << " complete, rv=" << rv;
  if (rv != net::OK) {
    // An error occurred while sending the ping response.
    DVLOG(1) << "Error sending " << ToString(message_type);
    logger_->LogSocketEventWithRv(socket_->id(), ChannelEvent::PING_WRITE_ERROR,
                                  rv);
    on_error_cb_.Run(ChannelError::CAST_SOCKET_ERROR);
    return;
  }

  if (liveness_timer_)
    liveness_timer_->Reset();
}

void KeepAliveHandler::LivenessTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_error_cb_.Run(ChannelError::PING_TIMEOUT);
  Stop();
}

void KeepAliveHandler::Stop() {
  if (started_) {
    started_ = false;
    ping_timer_->Stop();
    liveness_timer_->Stop();
  }
}

}  // namespace cast_channel
