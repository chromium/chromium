// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_
#define COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "third_party/openscreen/src/cast/streaming/sender_message.h"
#include "third_party/openscreen/src/cast/streaming/session_messenger.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace mirroring {

// Implementation of the RpcDispatcher API using an
// `openscreen::cast::SenderSessionMessenger` as the backing implementation.
class COMPONENT_EXPORT(MIRRORING_SERVICE) OpenscreenRpcDispatcher final
    : public RpcDispatcher {
 public:
  // NOTE: the messenger is expected to outlive `this`.
  explicit OpenscreenRpcDispatcher(
      openscreen::cast::SenderSessionMessenger& messenger);
  OpenscreenRpcDispatcher(OpenscreenRpcDispatcher&&) = delete;
  OpenscreenRpcDispatcher(const OpenscreenRpcDispatcher&) = delete;
  OpenscreenRpcDispatcher& operator=(OpenscreenRpcDispatcher&&) = delete;
  OpenscreenRpcDispatcher& operator=(const OpenscreenRpcDispatcher&) = delete;
  ~OpenscreenRpcDispatcher() final;

  // RpcDispatcher overrides.
  void Subscribe(RpcDispatcher::ResponseCallback callback) override;
  void Unsubscribe() override;
  bool SendOutboundMessage(base::span<const uint8_t> message) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OpenscreenRpcDispatcherTest, ReceivesMessages);
  void OnMessage(
      openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message);

  raw_ref<openscreen::cast::SenderSessionMessenger> messenger_;
  RpcDispatcher::ResponseCallback callback_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_RPC_DISPATCHER_H_
