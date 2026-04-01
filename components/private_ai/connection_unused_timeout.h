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
#include "components/private_ai/error_code.h"

namespace private_ai {

// A decorator for `Connection` that adds an inactivity timeout after
// the connection construction.
class ConnectionUnusedTimeout : public Connection {
 public:
  ConnectionUnusedTimeout(std::unique_ptr<Connection> inner_connection,
                          base::OnceCallback<void(ErrorCode)> on_disconnect,
                          base::TimeDelta unused_timeout);
  ~ConnectionUnusedTimeout() override;

  ConnectionUnusedTimeout(const ConnectionUnusedTimeout&) = delete;
  ConnectionUnusedTimeout& operator=(const ConnectionUnusedTimeout&) = delete;

  // Connection override:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(ErrorCode error) override;

 private:
  void OnUnusedTimeout();

  base::OneShotTimer unused_timer_;

  std::unique_ptr<Connection> inner_connection_;
  base::OnceCallback<void(ErrorCode)> on_disconnect_;

  base::WeakPtrFactory<ConnectionUnusedTimeout> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_UNUSED_TIMEOUT_H_
