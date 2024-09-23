// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/websocket_sb_extensions_handshake_throttle.h"

#include "base/metrics/histogram_functions.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"

namespace safe_browsing {

WebSocketSBExtensionsHandshakeThrottle::WebSocketSBExtensionsHandshakeThrottle(
    mojom::ExtensionWebRequestReporter* extension_web_request_reporter)
    : extension_web_request_reporter_(
          std::move(extension_web_request_reporter)) {}

WebSocketSBExtensionsHandshakeThrottle::
    ~WebSocketSBExtensionsHandshakeThrottle() = default;

void WebSocketSBExtensionsHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    const blink::WebSecurityOrigin& isolated_world_origin,
    blink::WebSocketHandshakeThrottle::OnCompletion completion_callback) {
  url_ = url;

  MaybeSendExtensionWebRequestData(url, creator_origin, isolated_world_origin);

  std::move(completion_callback).Run(std::nullopt);
  // |this| is destroyed here.
}

void WebSocketSBExtensionsHandshakeThrottle::MaybeSendExtensionWebRequestData(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    const blink::WebSecurityOrigin& isolated_world_origin) {
  // Skip if request destination isn't WS/WSS (ex. extension scheme).
  if (!url_.SchemeIsWSOrWSS()) {
    return;
  }

  if (!isolated_world_origin.IsNull() &&
      isolated_world_origin.Protocol() == extensions::kExtensionScheme) {
    // Logging "false" represents the data being *sent*.
    base::UmaHistogramBoolean(
        "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
        false);
    extension_web_request_reporter_->SendWebRequestData(
        isolated_world_origin.Host().Utf8().data(), url,
        mojom::WebRequestProtocolType::kWebSocket,
        mojom::WebRequestContactInitiatorType::kContentScript);
  } else if (creator_origin.Protocol() == extensions::kExtensionScheme) {
    // Logging "false" represents the data being *sent*.
    base::UmaHistogramBoolean(
        "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
        false);
    extension_web_request_reporter_->SendWebRequestData(
        creator_origin.Host().Utf8().data(), url,
        mojom::WebRequestProtocolType::kWebSocket,
        mojom::WebRequestContactInitiatorType::kExtension);
  }
}

}  // namespace safe_browsing
