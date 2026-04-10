// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_UNUSED_TIMEOUT_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_UNUSED_TIMEOUT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/status_code.h"

namespace private_ai {

// A decorator for `Connection` that adds an inactivity timeout after
// the connection construction. The timer is reset after each message
// is sent. If no message is sent within the timeout period, the
// connection is closed.
class ConnectionUnusedTimeout : public Connection {
 public:
  ConnectionUnusedTimeout(std::unique_ptr<Connection> inner_connection,
                          base::OnceCallback<void(StatusCode)> on_disconnect,
                          base::TimeDelta unused_timeout);
  ~ConnectionUnusedTimeout() override;

  ConnectionUnusedTimeout(const ConnectionUnusedTimeout&) = delete;
  ConnectionUnusedTimeout& operator=(const ConnectionUnusedTimeout&) = delete;

  // Connection override:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(StatusCode status_code) override;

 private:
  void OnUnusedTimeout();

  base::OneShotTimer unused_timer_;

  std::unique_ptr<Connection> inner_connection_;
  base::OnceCallback<void(StatusCode)> on_disconnect_;

  base::WeakPtrFactory<ConnectionUnusedTimeout> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_UNUSED_TIMEOUT_H_
