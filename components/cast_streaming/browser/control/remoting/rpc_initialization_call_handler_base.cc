// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/rpc_initialization_call_handler_base.h"

#include "base/functional/bind.h"
#include "media/cast/openscreen/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming::remoting {

RpcInitializationCallHandlerBase::RpcInitializationCallHandlerBase(
    RpcProcessMessageCB message_processor)
    : message_processor_(std::move(message_processor)), weak_factory_(this) {
  DCHECK(message_processor_);
}

RpcInitializationCallHandlerBase::~RpcInitializationCallHandlerBase() = default;

void RpcInitializationCallHandlerBase::OnRpcAcquireRenderer(
    openscreen::cast::RpcMessenger::Handle handle) {
  RpcAcquireRendererAsync(
      handle,
      base::BindOnce(&RpcInitializationCallHandlerBase::OnAcquireRendererDone,
                     weak_factory_.GetWeakPtr(), handle));
}

void RpcInitializationCallHandlerBase::OnAcquireRendererDone(
    openscreen::cast::RpcMessenger::Handle sender_handle,
    openscreen::cast::RpcMessenger::Handle receiver_handle) {
  auto message =
      media::cast::CreateMessageForAcquireRendererDone(receiver_handle);
  message_processor_.Run(sender_handle, std::move(message));
}

}  // namespace cast_streaming::remoting
