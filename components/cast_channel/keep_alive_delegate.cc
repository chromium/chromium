// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/keep_alive_delegate.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/logger.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

KeepAliveDelegate::KeepAliveDelegate(
    CastSocket* socket,
    scoped_refptr<Logger> logger,
    std::unique_ptr<CastTransport::Delegate> inner_delegate,
    base::TimeDelta ping_interval,
    base::TimeDelta liveness_timeout)
    : inner_delegate_(std::move(inner_delegate)),
      handler_(socket,
               std::move(logger),
               ping_interval,
               liveness_timeout,
               base::BindRepeating(&KeepAliveDelegate::OnError,
                                   base::Unretained(this))) {
  DCHECK(inner_delegate_);
}

KeepAliveDelegate::~KeepAliveDelegate() = default;

void KeepAliveDelegate::SetTimersForTest(
    std::unique_ptr<base::RetainingOneShotTimer> injected_ping_timer,
    std::unique_ptr<base::RetainingOneShotTimer> injected_liveness_timer) {
  handler_.SetTimersForTest(std::move(injected_ping_timer),
                            std::move(injected_liveness_timer));
}

// CastTransport::Delegate interface.
void KeepAliveDelegate::Start() {
  handler_.Start();
  inner_delegate_->Start();
}

void KeepAliveDelegate::OnError(ChannelError error_state) {
  DVLOG(1) << "KeepAlive::OnError: "
           << ::cast_channel::ChannelErrorToString(error_state);
  inner_delegate_->OnError(error_state);
  handler_.Stop();
}

void KeepAliveDelegate::OnMessage(const CastMessage& message) {
  DVLOG(2) << "KeepAlive::OnMessage : " << message.payload_utf8();
  if (!handler_.HandleMessage(message)) {
    inner_delegate_->OnMessage(message);
  }
}

}  // namespace cast_channel
