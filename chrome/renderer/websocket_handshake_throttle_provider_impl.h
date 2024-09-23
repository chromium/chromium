// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_
#define CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

// This must be constructed on the render thread, and then used and destructed
// on a single thread, which can be different from the render thread.
class WebSocketHandshakeThrottleProviderImpl final
    : public blink::WebSocketHandshakeThrottleProvider {
 public:
  explicit WebSocketHandshakeThrottleProviderImpl(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  WebSocketHandshakeThrottleProviderImpl& operator=(
      const WebSocketHandshakeThrottleProviderImpl&) = delete;

  ~WebSocketHandshakeThrottleProviderImpl() override;

  // Implements blink::WebSocketHandshakeThrottleProvider.
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  WebSocketHandshakeThrottleProviderImpl(
      const WebSocketHandshakeThrottleProviderImpl& other);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
      pending_extension_web_request_reporter_;
  mojo::Remote<safe_browsing::mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_;
#endif

  THREAD_CHECKER(thread_checker_);
};

#endif  // CHROME_RENDERER_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_IMPL_H_
