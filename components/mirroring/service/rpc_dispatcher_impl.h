// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_
#define COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/rpc_dispatcher.h"

namespace mirroring {

// Implementation of the RpcDispatcher API using a MessageDispatcher as the
// backing implementation.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RpcDispatcherImpl final
    : public RpcDispatcher {
 public:
  explicit RpcDispatcherImpl(MessageDispatcher& message_dispatcher);
  RpcDispatcherImpl(RpcDispatcherImpl&&) = delete;
  RpcDispatcherImpl(const RpcDispatcherImpl&) = delete;
  RpcDispatcherImpl& operator=(RpcDispatcherImpl&&) = delete;
  RpcDispatcherImpl& operator=(const RpcDispatcherImpl&) = delete;
  ~RpcDispatcherImpl() final;

  // RpcDispatcher overrides.
  void Subscribe(RpcDispatcher::ResponseCallback callback) override;
  void Unsubscribe() override;
  bool SendOutboundMessage(base::span<const uint8_t> message) override;

 private:
  void ProcessResponse(RpcDispatcher::ResponseCallback callback,
                       const ReceiverResponse& response);

  raw_ref<MessageDispatcher, DanglingUntriaged> message_dispatcher_;
  bool is_subscribed_ = false;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RPC_DISPATCHER_IMPL_H_
