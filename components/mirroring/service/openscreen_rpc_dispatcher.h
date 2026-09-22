// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_
#define COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_message.h"
#include "third_party/openscreen/src/cast/streaming/public/session_messenger.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace mirroring {

// Dispatches RPC messages using an
// openscreen::cast::SenderSessionMessenger.
class COMPONENT_EXPORT(MIRRORING_SERVICE) OpenscreenRpcDispatcher final
    : public RpcDispatcher {
 public:
  OpenscreenRpcDispatcher();
  explicit OpenscreenRpcDispatcher(
      openscreen::cast::SenderSessionMessenger& messenger);
  OpenscreenRpcDispatcher(const OpenscreenRpcDispatcher&) = delete;
  OpenscreenRpcDispatcher& operator=(const OpenscreenRpcDispatcher&) = delete;
  ~OpenscreenRpcDispatcher() override;

  // RpcDispatcher implementation.
  void Subscribe(ResponseCallback callback,
                 ErrorCallback error_callback) override;
  void Unsubscribe() override;
  bool SendOutboundMessage(base::span<const uint8_t> message) override;

 private:
  void OnMessage(
      openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message);

  // The messenger is owned by `openscreen::cast::SenderSession` in
  // `OpenscreenSessionHost`, which outlives `OpenscreenRpcDispatcher` (in
  // `OpenscreenSessionHost`, `rpc_dispatcher_` is reset before `session_` both
  // in `StopSession()` and via reverse member destruction in the destructor).
  // This is a `raw_ptr` rather than `raw_ref` to permit default construction
  // for tests.
  raw_ptr<openscreen::cast::SenderSessionMessenger> messenger_ = nullptr;
  ResponseCallback callback_;
  ErrorCallback error_callback_;

  base::WeakPtrFactory<OpenscreenRpcDispatcher> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_
