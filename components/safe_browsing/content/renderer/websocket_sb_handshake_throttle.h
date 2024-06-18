// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SafeBrowsing hook for WebSockets, used for extension telemetry. This code
// runs inside the render process, calling the interface defined in
// safe_browsing.mojom to communicate with the extension telemetry service.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

class WebSocketSBHandshakeThrottle : public blink::WebSocketHandshakeThrottle {
 public:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // |extension_web_request_reporter_pending_remote| is used for sending
  // extension web requests to the browser.
  explicit WebSocketSBHandshakeThrottle(
      mojom::ExtensionWebRequestReporter* extension_web_request_reporter);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  WebSocketSBHandshakeThrottle(const WebSocketSBHandshakeThrottle&) = delete;
  WebSocketSBHandshakeThrottle& operator=(const WebSocketSBHandshakeThrottle&) =
      delete;

  ~WebSocketSBHandshakeThrottle() override;

  void ThrottleHandshake(const blink::WebURL& url,
                         const blink::WebSecurityOrigin& creator_origin,
                         const blink::WebSecurityOrigin& isolated_world_origin,
                         blink::WebSocketHandshakeThrottle::OnCompletion
                             completion_callback) override;

 private:
  GURL url_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Send web request data to the browser if the request
  // originated from an extension and destination is WS/WSS scheme only.
  void MaybeSendExtensionWebRequestData(
      const blink::WebURL& url,
      const blink::WebSecurityOrigin& creator_origin,
      const blink::WebSecurityOrigin& isolated_world_origin);

  raw_ptr<mojom::ExtensionWebRequestReporter> extension_web_request_reporter_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
