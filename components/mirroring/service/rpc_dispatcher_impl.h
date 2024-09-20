// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_
#define COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_message.h"
#include "third_party/openscreen/src/cast/streaming/public/session_messenger.h"
#include "third_party/openscreen/src/cast/streaming/sender_message.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace mirroring {

// Service for dispatching inbound and outbound RPC messages using an
// openscreen::cast::SenderSessionMessenger.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RpcDispatcherImpl
    : public RpcDispatcher {
 public:
  explicit RpcDispatcherImpl(
      openscreen::cast::SenderSessionMessenger& messenger);
  RpcDispatcherImpl(const RpcDispatcherImpl&) = delete;
  RpcDispatcherImpl(RpcDispatcherImpl&&) = delete;
  RpcDispatcherImpl& operator=(const RpcDispatcherImpl&) = delete;
  RpcDispatcherImpl& operator=(RpcDispatcherImpl&&) = delete;
  ~RpcDispatcherImpl() override;

  // Handles registration for RPC messages. Currently only one callback that
  // receives all RPC messages is allowed. Multiple calls to `Subscribe` will
  // overwrite the currently set callback. Callbacks should be executed
  // on the same sequence that RpcDispatcherImpl is instantiated.
  void Subscribe(ResponseCallback callback) final;
  void Unsubscribe() final;

  // Requests to send outbound `message` to the remoting implementation on the
  // receiver. The message is routed based on the already encoded RPC message
  // `handle`.
  //
  // Returns `true` if the message was sent (or queued to be sent
  // asynchronously) successfully.
  bool SendOutboundMessage(base::span<const uint8_t> message) final;

 private:
  FRIEND_TEST_ALL_PREFIXES(RpcDispatcherImplTest, ReceivesMessages);

  void OnMessage(
      openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message);

  raw_ref<openscreen::cast::SenderSessionMessenger> messenger_;
  ResponseCallback callback_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_
