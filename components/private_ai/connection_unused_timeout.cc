// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_unused_timeout.h"

#include <utility>

#include "base/functional/bind.h"

namespace private_ai {

ConnectionUnusedTimeout::ConnectionUnusedTimeout(
    std::unique_ptr<Connection> inner_connection,
    base::OnceCallback<void(StatusCode)> on_disconnect,
    base::TimeDelta unused_timeout)
    : inner_connection_(std::move(inner_connection)),
      on_disconnect_(std::move(on_disconnect)) {
  unused_timer_.Start(FROM_HERE, unused_timeout,
                      base::BindOnce(&ConnectionUnusedTimeout::OnUnusedTimeout,
                                     weak_factory_.GetWeakPtr()));
}

ConnectionUnusedTimeout::~ConnectionUnusedTimeout() = default;

void ConnectionUnusedTimeout::Send(proto::PrivateAiRequest request,
                                   base::TimeDelta timeout,
                                   OnRequestCallback callback) {
  unused_timer_.Reset();
  inner_connection_->Send(std::move(request), timeout, std::move(callback));
}

void ConnectionUnusedTimeout::OnDestroy(StatusCode status_code) {
  unused_timer_.Stop();
  inner_connection_->OnDestroy(status_code);
}

void ConnectionUnusedTimeout::OnUnusedTimeout() {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run(StatusCode::kUnusedConnection);
  }
}

}  // namespace private_ai
