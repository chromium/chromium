// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_
#define CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

// This must be constructed on the render thread, and then used and destructed
// on a single thread, which can be different from the render thread.
class WebSocketHandshakeThrottleProviderImpl final
    : public content::WebSocketHandshakeThrottleProvider {
 public:
  explicit WebSocketHandshakeThrottleProviderImpl(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);
  ~WebSocketHandshakeThrottleProviderImpl() override;

  // Implements content::WebSocketHandshakeThrottleProvider.
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      int render_frame_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  WebSocketHandshakeThrottleProviderImpl(
      const WebSocketHandshakeThrottleProviderImpl& other);

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> safe_browsing_remote_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(WebSocketHandshakeThrottleProviderImpl);
};

#endif  // CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_
