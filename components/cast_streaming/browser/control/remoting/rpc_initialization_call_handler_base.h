// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/openscreen/rpc_call_message_handler.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// This class acts to simplify the initialization process. Instead of directly
// handling RPC messages, implementers of this class only need to call a single
// callback at the appropriate time.
class RpcInitializationCallHandlerBase
    : public media::cast::RpcInitializationCallMessageHandler {
 public:
  ~RpcInitializationCallHandlerBase() override;

  // Called following an RPC call to acquire the Renderer. Callback parameter is
  // the handle to be used by future calls associated with the Renderer.
  using AcquireRendererCB =
      base::OnceCallback<void(openscreen::cast::RpcMessenger::Handle)>;
  virtual void RpcAcquireRendererAsync(
      openscreen::cast::RpcMessenger::Handle remote_handle,
      AcquireRendererCB cb) = 0;

 protected:
  using RpcProcessMessageCB = base::RepeatingCallback<void(
      openscreen::cast::RpcMessenger::Handle handle,
      std::unique_ptr<openscreen::cast::RpcMessage>)>;
  explicit RpcInitializationCallHandlerBase(
      RpcProcessMessageCB message_processor);

 private:
  void OnAcquireRendererDone(
      openscreen::cast::RpcMessenger::Handle sender_handle,
      openscreen::cast::RpcMessenger::Handle receiver_handle);

  // RpcInitializationCallMessageHandler partial implementation.
  void OnRpcAcquireRenderer(
      openscreen::cast::RpcMessenger::Handle sender_handle) override;

  RpcProcessMessageCB message_processor_;

  base::WeakPtrFactory<RpcInitializationCallHandlerBase> weak_factory_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_
