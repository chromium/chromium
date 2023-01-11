// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_H_
#define COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_H_

#include <stdint.h>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"

namespace mirroring {

// Abstract interface for dispatching inbound and outbound RPC messages.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RpcDispatcher {
 public:
  RpcDispatcher() = default;
  RpcDispatcher(const RpcDispatcher&) = delete;
  RpcDispatcher(RpcDispatcher&&) = delete;
  RpcDispatcher& operator=(const RpcDispatcher&) = delete;
  RpcDispatcher& operator=(RpcDispatcher&&) = delete;
  virtual ~RpcDispatcher() = default;

  using ResponseCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& response)>;

  // Handles registration for RPC messages. Currently only one callback that
  // receives all RPC messages is allowed. Multiple calls to `Subscribe` will
  // overwrite the currently set callback. Callbacks should be executed
  // on the same sequence that RpcDispatcher is instantiated.
  virtual void Subscribe(ResponseCallback callback) = 0;
  virtual void Unsubscribe() = 0;

  // Requests to send outbound `message` to the remoting implementation on the
  // receiver. The message is routed based on the already encoded RPC message
  // `handle`.
  //
  // Returns `true` if the message was sent (or queued to be sent
  // asynchronously) successfully.
  virtual bool SendOutboundMessage(base::span<const uint8_t> message) = 0;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_H_
