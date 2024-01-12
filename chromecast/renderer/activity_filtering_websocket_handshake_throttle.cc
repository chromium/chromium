// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/activity_filtering_websocket_handshake_throttle.h"

#include "base/strings/stringprintf.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/gurl.h"

namespace chromecast {

ActivityFilteringWebSocketHandshakeThrottle::
    ActivityFilteringWebSocketHandshakeThrottle(ActivityUrlFilter* filter)
    : url_filter_(filter) {}

ActivityFilteringWebSocketHandshakeThrottle::
    ~ActivityFilteringWebSocketHandshakeThrottle() = default;

void ActivityFilteringWebSocketHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    const blink::WebSecurityOrigin& isolated_world_origin,
    blink::WebSocketHandshakeThrottle::OnCompletion completion_callback) {
  GURL gurl = GURL(url);

  // Pass through allowed URLs, block otherwise.
  if (url_filter_->UrlMatchesWhitelist(gurl)) {
    std::move(completion_callback).Run(std::nullopt);
    return;
  }

  std::move(completion_callback)
      .Run(blink::WebString::FromUTF8(base::StringPrintf(
          "WebSocket connection to %s is blocked", gurl.spec().c_str())));
}

}  // namespace chromecast
