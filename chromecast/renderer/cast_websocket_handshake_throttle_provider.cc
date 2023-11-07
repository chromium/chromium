// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_websocket_handshake_throttle_provider.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/renderer/activity_filtering_websocket_handshake_throttle.h"
#include "services/network/public/cpp/features.h"

namespace chromecast {

CastWebSocketHandshakeThrottleProvider::CastWebSocketHandshakeThrottleProvider(
    CastActivityUrlFilterManager* url_filter_manager)
    : cast_activity_url_filter_manager_(url_filter_manager) {
  DCHECK(url_filter_manager);
  DETACH_FROM_THREAD(thread_checker_);
}

CastWebSocketHandshakeThrottleProvider::
    ~CastWebSocketHandshakeThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

CastWebSocketHandshakeThrottleProvider::CastWebSocketHandshakeThrottleProvider(
    const chromecast::CastWebSocketHandshakeThrottleProvider& other)
    : cast_activity_url_filter_manager_(
          other.cast_activity_url_filter_manager_) {
  DETACH_FROM_THREAD(thread_checker_);
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
CastWebSocketHandshakeThrottleProvider::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new CastWebSocketHandshakeThrottleProvider(*this));
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
CastWebSocketHandshakeThrottleProvider::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!local_frame_token.has_value()) {
    return nullptr;
  }

  auto* activity_url_filter =
      cast_activity_url_filter_manager_
          ->GetActivityUrlFilterForRenderFrameToken(local_frame_token.value());
  if (!activity_url_filter)
    return nullptr;

  return std::make_unique<ActivityFilteringWebSocketHandshakeThrottle>(
      activity_url_filter);
}

}  // namespace chromecast
