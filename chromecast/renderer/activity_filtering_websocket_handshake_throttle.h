// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_ACTIVITY_FILTERING_WEBSOCKET_HANDSHAKE_THROTTLE_H_
#define CHROMECAST_RENDERER_ACTIVITY_FILTERING_WEBSOCKET_HANDSHAKE_THROTTLE_H_

#include "chromecast/common/activity_url_filter.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

namespace chromecast {

// This class implements the WebSocketHandshakeThrottle class as a facility
// to block WebSocket connection establishment. Specifically,
// blink::Platform::CreateWebSocketHandshakeThrottle() is called when a
// WebSocket handshake is started. If
// ActivityFilteringWebSocketHandshakeThrottle is installed, the
// ThrottleHandshake() will be called on the handshake. If the URL is not
// whitelisted, the handshake will be aborted, and a connection error will be
// reported to Javascript.
class ActivityFilteringWebSocketHandshakeThrottle
    : public blink::WebSocketHandshakeThrottle {
 public:
  explicit ActivityFilteringWebSocketHandshakeThrottle(
      ActivityUrlFilter* filter);

  ActivityFilteringWebSocketHandshakeThrottle(
      const ActivityFilteringWebSocketHandshakeThrottle&) = delete;
  ActivityFilteringWebSocketHandshakeThrottle& operator=(
      const ActivityFilteringWebSocketHandshakeThrottle&) = delete;

  ~ActivityFilteringWebSocketHandshakeThrottle() override;

  // blink::WebSocketHandshakeThrottle implementation:
  void ThrottleHandshake(const blink::WebURL& url,
                         const blink::WebSecurityOrigin& creator_origin,
                         const blink::WebSecurityOrigin& isolated_world_origin,
                         blink::WebSocketHandshakeThrottle::OnCompletion
                             completion_callback) override;

 private:
  ActivityUrlFilter* const url_filter_;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_ACTIVITY_FILTERING_WEBSOCKET_HANDSHAKE_THROTTLE_H_
