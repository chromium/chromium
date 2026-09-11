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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_message.h"
#include "third_party/openscreen/src/cast/streaming/public/session_messenger.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace mirroring {

// Service for dispatching inbound and outbound RPC messages using an
// openscreen::cast::SenderSessionMessenger.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RpcDispatcher {
 public:
  RpcDispatcher();
  explicit RpcDispatcher(openscreen::cast::SenderSessionMessenger& messenger);
  RpcDispatcher(const RpcDispatcher&) = delete;
  RpcDispatcher(RpcDispatcher&&) = delete;
  RpcDispatcher& operator=(const RpcDispatcher&) = delete;
  RpcDispatcher& operator=(RpcDispatcher&&) = delete;
  virtual ~RpcDispatcher();

  using ResponseCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& response)>;
  using ErrorCallback = base::RepeatingClosure;

  // Handles registration for RPC messages. Currently only one callback that
  // receives all RPC messages is allowed. Multiple calls to `Subscribe` will
  // overwrite the currently set callbacks. Callbacks should be executed
  // on the same sequence that RpcDispatcher is instantiated.
  virtual void Subscribe(ResponseCallback callback,
                         ErrorCallback error_callback);
  virtual void Unsubscribe();

  // Requests to send outbound `message` to the remoting implementation on the
  // receiver. The message is routed based on the already encoded RPC message
  // `handle`.
  //
  // Returns `true` if the message was sent (or queued to be sent
  // asynchronously) successfully.
  virtual bool SendOutboundMessage(base::span<const uint8_t> message);

 private:
  void OnMessage(
      openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message);

  // The messenger is owned by `openscreen::cast::SenderSession` in
  // `OpenscreenSessionHost`, which outlives `RpcDispatcher` (in
  // `OpenscreenSessionHost`, `rpc_dispatcher_` is reset before `session_` both
  // in `StopSession()` and via reverse member destruction in the destructor).
  // This is a `raw_ptr` rather than `raw_ref` to permit default construction
  // for test mocks (e.g. `MockRpcDispatcher`).
  raw_ptr<openscreen::cast::SenderSessionMessenger> messenger_ = nullptr;
  ResponseCallback callback_;
  ErrorCallback error_callback_;

  base::WeakPtrFactory<RpcDispatcher> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_H_
