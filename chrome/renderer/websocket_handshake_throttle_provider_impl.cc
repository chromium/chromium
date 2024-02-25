// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/content/renderer/websocket_sb_handshake_throttle.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

WebSocketHandshakeThrottleProviderImpl::WebSocketHandshakeThrottleProviderImpl(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  DETACH_FROM_THREAD(thread_checker_);
  broker->GetInterface(pending_safe_browsing_.InitWithNewPipeAndPassReceiver());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  broker->GetInterface(
      pending_extension_web_request_reporter_.InitWithNewPipeAndPassReceiver());
#endif
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
      pending_safe_browsing_.InitWithNewPipeAndPassReceiver());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(other.extension_web_request_reporter_);
  other.extension_web_request_reporter_->Clone(
      pending_extension_web_request_reporter_.InitWithNewPipeAndPassReceiver());
#endif
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
WebSocketHandshakeThrottleProviderImpl::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pending_safe_browsing_) {
    safe_browsing_.Bind(std::move(pending_safe_browsing_), task_runner);
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pending_extension_web_request_reporter_) {
    extension_web_request_reporter_.Bind(
        std::move(pending_extension_web_request_reporter_), task_runner);
  }
#endif
  return base::WrapUnique(new WebSocketHandshakeThrottleProviderImpl(*this));
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
WebSocketHandshakeThrottleProviderImpl::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pending_safe_browsing_) {
    safe_browsing_.Bind(std::move(pending_safe_browsing_),
                        std::move(task_runner));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pending_extension_web_request_reporter_) {
    extension_web_request_reporter_.Bind(
        std::move(pending_extension_web_request_reporter_));
  }
  auto throttle = std::make_unique<safe_browsing::WebSocketSBHandshakeThrottle>(
      safe_browsing_.get(), local_frame_token,
      extension_web_request_reporter_.get());
#else
  auto throttle = std::make_unique<safe_browsing::WebSocketSBHandshakeThrottle>(
      safe_browsing_.get(), local_frame_token);
#endif
  return throttle;
}
