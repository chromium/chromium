// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SafeBrowsing hook for WebSockets, used for extension telemetry. This code
// runs inside the render process, calling the interface defined in
// safe_browsing.mojom to communicate with the extension telemetry service.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_EXTENSIONS_HANDSHAKE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_EXTENSIONS_HANDSHAKE_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

class WebSocketSBExtensionsHandshakeThrottle
    : public blink::WebSocketHandshakeThrottle {
 public:
  // |extension_web_request_reporter_pending_remote| is used for sending
  // extension web requests to the browser.
  explicit WebSocketSBExtensionsHandshakeThrottle(
      mojom::ExtensionWebRequestReporter* extension_web_request_reporter);

  WebSocketSBExtensionsHandshakeThrottle(
      const WebSocketSBExtensionsHandshakeThrottle&) = delete;
  WebSocketSBExtensionsHandshakeThrottle& operator=(
      const WebSocketSBExtensionsHandshakeThrottle&) = delete;

  ~WebSocketSBExtensionsHandshakeThrottle() override;

  void ThrottleHandshake(const blink::WebURL& url,
                         const blink::WebSecurityOrigin& creator_origin,
                         const blink::WebSecurityOrigin& isolated_world_origin,
                         blink::WebSocketHandshakeThrottle::OnCompletion
                             completion_callback) override;

 private:
  GURL url_;

  // Send web request data to the browser if the request
  // originated from an extension and destination is WS/WSS scheme only.
  void MaybeSendExtensionWebRequestData(
      const blink::WebURL& url,
      const blink::WebSecurityOrigin& creator_origin,
      const blink::WebSecurityOrigin& isolated_world_origin);

  raw_ptr<mojom::ExtensionWebRequestReporter> extension_web_request_reporter_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_EXTENSIONS_HANDSHAKE_THROTTLE_H_
