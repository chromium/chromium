// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_

#include "base/timer/timer.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "components/media_router/common/providers/cast/channel/keep_alive_handler.h"

namespace cast_channel {

class CastSocket;
class Logger;

using ::openscreen::cast::proto::CastMessage;

// Decorator delegate which provides keep-alive functionality.
// Keep-alive messages are handled by this object; all other messages and
// errors are passed to |inner_delegate_|.
class KeepAliveDelegate : public CastTransport::Delegate {
 public:
  // |socket|: The socket to be kept alive.
  // |logger|: The logging object which collects protocol events and error
  //           details.
  // |inner_delegate|: The delegate which processes all non-keep-alive
  //                   messages. This object assumes ownership of
  //                   |inner_delegate|.
  // |ping_interval|: The amount of idle time to wait before sending a PING to
  //                  the remote end.
  // |liveness_timeout|: The amount of idle time to wait before terminating the
  //                     connection.
  KeepAliveDelegate(CastSocket* socket,
                    scoped_refptr<Logger> logger,
                    std::unique_ptr<CastTransport::Delegate> inner_delegate,
                    base::TimeDelta ping_interval,
                    base::TimeDelta liveness_timeout);

  KeepAliveDelegate(const KeepAliveDelegate&) = delete;
  KeepAliveDelegate& operator=(const KeepAliveDelegate&) = delete;

  ~KeepAliveDelegate() override;

  void SetTimersForTest(
      std::unique_ptr<base::RetainingOneShotTimer> injected_ping_timer,
      std::unique_ptr<base::RetainingOneShotTimer> injected_liveness_timer);

  // CastTransport::Delegate implementation.
  void Start() override;
  void OnError(ChannelError error_state) override;
  void OnMessage(const CastMessage& message) override;

 private:
  // Delegate object which receives all non-keep alive messages.
  std::unique_ptr<CastTransport::Delegate> inner_delegate_;

  KeepAliveHandler handler_;
};

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_
