// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_KEEP_ALIVE_HANDLER_H_
#define COMPONENTS_CAST_CHANNEL_KEEP_ALIVE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_message_util.h"

namespace cast_channel {

class CastSocket;
class Logger;

using ::cast::channel::CastMessage;

class KeepAliveHandler {
 public:
  using OnErrorCallback = base::RepeatingCallback<void(ChannelError)>;
  // |socket|: The socket to be kept alive.
  // |logger|: The logging object which collects protocol events and error
  //           details.
  // |ping_interval|: The amount of idle time to wait before sending a PING to
  //                  the remote end.
  // |liveness_timeout|: The amount of idle time to wait before terminating the
  //                     connection.
  KeepAliveHandler(CastSocket* socket,
                   scoped_refptr<Logger> logger,
                   base::TimeDelta ping_interval,
                   base::TimeDelta liveness_timeout,
                   OnErrorCallback on_error_cb);
  ~KeepAliveHandler();

  // Restarts the ping/liveness timeout timers. Called when a message
  // is received from the remote end.
  void ResetTimers();

  void SetTimersForTest(
      std::unique_ptr<base::RetainingOneShotTimer> injected_ping_timer,
      std::unique_ptr<base::RetainingOneShotTimer> injected_liveness_timer);

  void Start();

  // Stops the ping and liveness timers if they are started.
  // To be called after an error.
  void Stop();

  bool HandleMessage(const CastMessage& message);

 private:
  // Sends a formatted PING or PONG message to the remote side.
  void SendKeepAliveMessage(const CastMessage& message,
                            CastMessageType message_type);

  // Callback for SendKeepAliveMessage.
  void SendKeepAliveMessageComplete(CastMessageType message_type, int rv);

  // Called when the liveness timer expires, indicating that the remote
  // end has not responded within the |liveness_timeout_| interval.
  void LivenessTimeout();

  // Indicates that Start() was called.
  bool started_;

  // Socket that is managed by the keep-alive object.
  CastSocket* socket_;

  // Logging object.
  scoped_refptr<Logger> logger_;

  // Amount of idle time to wait before disconnecting.
  base::TimeDelta liveness_timeout_;

  // Amount of idle time to wait before pinging the receiver.
  base::TimeDelta ping_interval_;

  // Fired when |ping_interval_| is exceeded or when triggered by test code.
  std::unique_ptr<base::RetainingOneShotTimer> ping_timer_;

  // Fired when |liveness_timer_| is exceeded.
  std::unique_ptr<base::RetainingOneShotTimer> liveness_timer_;

  // The PING message to send over the wire.
  const CastMessage ping_message_;

  // The PONG message to send over the wire.
  const CastMessage pong_message_;

  OnErrorCallback on_error_cb_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<KeepAliveHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KeepAliveHandler);
};

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_KEEP_ALIVE_HANDLER_H_
