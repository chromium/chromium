// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rpc_dispatcher_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mirroring {

RpcDispatcherImpl::RpcDispatcherImpl(
    openscreen::cast::SenderSessionMessenger& messenger)
    : messenger_(messenger) {}

RpcDispatcherImpl::~RpcDispatcherImpl() {
  Unsubscribe();
}

void RpcDispatcherImpl::Subscribe(RpcDispatcher::ResponseCallback callback) {
  callback_ = std::move(callback);

  messenger_->SetHandler(
      openscreen::cast::ReceiverMessage::Type::kRpc,
      // Use of `this` is safe because we unsubscribe on destruction.
      [this](openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message) {
        OnMessage(std::move(message));
      });
}

void RpcDispatcherImpl::Unsubscribe() {
  if (callback_) {
    callback_.Reset();
    messenger_->ResetHandler(openscreen::cast::ReceiverMessage::Type::kRpc);
  }
}

bool RpcDispatcherImpl::SendOutboundMessage(base::span<const uint8_t> message) {
  const openscreen::Error error = messenger_->SendRpcMessage(
      std::vector<uint8_t>(message.begin(), message.end()));
  return error.ok();
}

void RpcDispatcherImpl::OnMessage(
    openscreen::ErrorOr<openscreen::cast::ReceiverMessage> message) {
  // TODO(crbug.com/40237878): RpcDispatcherImpl should have error
  // reporting.
  if (message.is_error()) {
    DLOG(ERROR) << __func__
                << ": had a message error: " << message.error().ToString();
    return;
  }
  DCHECK_EQ(openscreen::cast::ReceiverMessage::Type::kRpc,
            message.value().type);

  // We may get messages before subscription is completed.
  if (callback_) {
    callback_.Run(absl::get<std::vector<uint8_t>>(message.value().body));
  } else {
    DVLOG(1) << __func__ << ": received a message but no callback registered.";
  }
}

}  // namespace mirroring
