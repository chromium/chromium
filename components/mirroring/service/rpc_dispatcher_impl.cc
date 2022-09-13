// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rpc_dispatcher_impl.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/rpc_dispatcher.h"

namespace mirroring {

RpcDispatcherImpl::RpcDispatcherImpl(MessageDispatcher& message_dispatcher)
    : message_dispatcher_(message_dispatcher) {}

RpcDispatcherImpl::~RpcDispatcherImpl() {
  if (is_subscribed_) {
    Unsubscribe();
  }
}

void RpcDispatcherImpl::Subscribe(RpcDispatcher::ResponseCallback callback) {
  // NOTE: use of `base::Unretained` is safe because we unsubscribe from the
  // `message_dispatcher_` in the destructor of this class.
  message_dispatcher_->Subscribe(
      ResponseType::RPC,
      base::BindRepeating(&RpcDispatcherImpl::ProcessResponse,
                          base::Unretained(this), std::move(callback)));
  is_subscribed_ = true;
}

void RpcDispatcherImpl::ProcessResponse(
    RpcDispatcher::ResponseCallback callback,
    const ReceiverResponse& response) {
  DCHECK_EQ(ResponseType::RPC, response.type());
  callback.Run(
      std::vector<uint8_t>(response.rpc().begin(), response.rpc().end()));
}

void RpcDispatcherImpl::Unsubscribe() {
  message_dispatcher_->Unsubscribe(ResponseType::RPC);
  is_subscribed_ = false;
}

bool RpcDispatcherImpl::SendOutboundMessage(base::span<const uint8_t> message) {
  std::string encoded_rpc;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(message.data()),
                        message.size()),
      &encoded_rpc);
  base::Value::Dict rpc;
  rpc.Set("type", "RPC");
  rpc.Set("rpc", std::move(encoded_rpc));
  mojom::CastMessagePtr rpc_message = mojom::CastMessage::New();
  rpc_message->message_namespace = mojom::kRemotingNamespace;
  const bool did_serialize_rpc =
      base::JSONWriter::Write(rpc, &rpc_message->json_format_data);
  DCHECK(did_serialize_rpc);
  message_dispatcher_->SendOutboundMessage(std::move(rpc_message));
  return true;
}

}  // namespace mirroring
