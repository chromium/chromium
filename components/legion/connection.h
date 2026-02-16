// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CONNECTION_H_
#define COMPONENTS_LEGION_CONNECTION_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/legion/error_code.h"
#include "components/legion/proto/legion.pb.h"

namespace private_ai {

// Interface for a connection between Chrome and Legion server,
// sending requests, and receiving responses.
//
// Implementations of this interface should follow RAII, meaning that actual
// "wire" connection to the server happens during construction of
// the Connection instance and destruction of Connection instance leads to
// disconnecting from a server. Also disconnection from a server can happen
// during Connection instance lifetime.
class Connection {
 public:
  using OnRequestCallback = base::OnceCallback<void(
      base::expected<proto::LegionResponse, ErrorCode> result)>;

  virtual ~Connection() = default;

  // Sends a request to the Legion server and invokes `callback` when a response
  // is received or an error occurs.
  //
  // `timeout` is a hint of how much time a caller is willing to wait for
  // a response.
  virtual void Send(proto::LegionRequest request,
                    base::TimeDelta timeout,
                    OnRequestCallback callback) = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_CONNECTION_H_
