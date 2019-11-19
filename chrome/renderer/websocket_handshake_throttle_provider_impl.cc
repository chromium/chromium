// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"

#include <utility>

#include "components/safe_browsing/renderer/websocket_sb_handshake_throttle.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

WebSocketHandshakeThrottleProviderImpl::WebSocketHandshakeThrottleProviderImpl(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  DETACH_FROM_THREAD(thread_checker_);
  broker->GetInterface(safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
}

WebSocketHandshakeThrottleProviderImpl::
    ~WebSocketHandshakeThrottleProviderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

WebSocketHandshakeThrottleProviderImpl::WebSocketHandshakeThrottleProviderImpl(
    const WebSocketHandshakeThrottleProviderImpl& other) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(other.safe_browsing_);
  other.safe_browsing_->Clone(
      safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
}

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
WebSocketHandshakeThrottleProviderImpl::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (safe_browsing_remote_)
    safe_browsing_.Bind(std::move(safe_browsing_remote_),
                        std::move(task_runner));
  return base::WrapUnique(new WebSocketHandshakeThrottleProviderImpl(*this));
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
WebSocketHandshakeThrottleProviderImpl::CreateThrottle(
    int render_frame_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (safe_browsing_remote_)
    safe_browsing_.Bind(std::move(safe_browsing_remote_),
                        std::move(task_runner));
  return std::make_unique<safe_browsing::WebSocketSBHandshakeThrottle>(
      safe_browsing_.get(), render_frame_id);
}
