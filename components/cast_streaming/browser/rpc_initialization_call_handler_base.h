// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/cast_streaming/public/rpc_call_message_handler.h"

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// This class acts to simplify the initialization process. Instead of directly
// handling RPC messages, implementers of this class only need to call a single
// callback at the appropriate time.
class RpcInitializationCallHandlerBase
    : public RpcInitializationCallMessageHandler {
 public:
  ~RpcInitializationCallHandlerBase() override;

  // Called following an RPC call to acquire the Renderer. Callback parameter is
  // the handle to be used by future calls associated with the Renderer.
  using AcquireRendererCB = base::OnceCallback<void(int)>;
  virtual void RpcAcquireRendererAsync(AcquireRendererCB cb) = 0;

 protected:
  using RpcProcessMessageCB = base::RepeatingCallback<
      void(int handle, std::unique_ptr<openscreen::cast::RpcMessage>)>;

  explicit RpcInitializationCallHandlerBase(
      RpcProcessMessageCB message_processor);

 private:
  void OnAcquireRendererDone(int sender_handle, int receiver_handle);

  // RpcInitializationCallMessageHandler partial implementation.
  void OnRpcAcquireRenderer(int sender_handle) override;

  RpcProcessMessageCB message_processor_;

  base::WeakPtrFactory<RpcInitializationCallHandlerBase> weak_factory_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RPC_INITIALIZATION_CALL_HANDLER_BASE_H_
