// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rpc_dispatcher.h"

#include <utility>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace mirroring {

RpcDispatcher::RpcDispatcher() = default;

RpcDispatcher::RpcDispatcher(
    openscreen::cast::SenderSessionMessenger& messenger)
    : messenger_(&messenger) {}

RpcDispatcher::~RpcDispatcher() {
  Unsubscribe();
}

void RpcDispatcher::Subscribe(ResponseCallback callback,
                              ErrorCallback error_callback) {
  callback_ = std::move(callback);
  error_callback_ = std::move(error_callback);

  if (messenger_) {
    messenger_->SetHandler(
        openscreen::cast::ReceiverMessage::Type::kRpc,
        [weak_this = weak_factory_.GetWeakPtr()](
            openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message) {
          if (weak_this) {
            weak_this->OnMessage(std::move(message));
          }
        });
  }
}

void RpcDispatcher::Unsubscribe() {
  callback_.Reset();
  error_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  if (messenger_) {
    messenger_->ResetHandler(openscreen::cast::ReceiverMessage::Type::kRpc);
  }
}

bool RpcDispatcher::SendOutboundMessage(base::span<const uint8_t> message) {
  if (!messenger_) {
    return false;
  }
  const openscreen::Error error = messenger_->SendRpcMessage(
      openscreen::ByteView(message.data(), message.size()));
  if (!error.ok()) {
    if (error_callback_) {
      ErrorCallback cb = error_callback_;
      cb.Run();
    }
  }
  return error.ok();
}

void RpcDispatcher::OnMessage(
    openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message) {
  if (message.is_error()) {
    if (error_callback_) {
      ErrorCallback cb = error_callback_;
      cb.Run();
    }
    return;
  }
  DCHECK_EQ(openscreen::cast::ReceiverMessage::Type::kRpc,
            message.value().type);

  // We may get messages before subscription is completed.
  if (callback_) {
    ResponseCallback cb = callback_;
    cb.Run(std::get<std::vector<uint8_t>>(message.value().body));
  }
}

}  // namespace mirroring
