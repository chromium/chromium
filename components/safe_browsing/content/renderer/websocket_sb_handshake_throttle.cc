// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/websocket_sb_handshake_throttle.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing/core/common/scheme_logger.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"

// TODO(crbug.com/40934395) [Also TODO(thefrog)]: Move entire file to
// ENABLE_EXTENSIONS-only build and remove in-code build checks.
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace safe_browsing {

// TODO(crbug.com/40934395) [Also TODO(thefrog)]: Remove constructor,
// `frame_token_`, and `safe_browsing_`.
WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token)
    : frame_token_(local_frame_token.CopyAsOptional()),
      safe_browsing_(safe_browsing) {}

#if BUILDFLAG(ENABLE_EXTENSIONS)
WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    mojom::ExtensionWebRequestReporter* extension_web_request_reporter)
    : frame_token_(local_frame_token.CopyAsOptional()),
      safe_browsing_(safe_browsing),
      extension_web_request_reporter_(
          std::move(extension_web_request_reporter)) {}
#endif

WebSocketSBHandshakeThrottle::~WebSocketSBHandshakeThrottle() = default;

void WebSocketSBHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    const blink::WebSecurityOrigin& isolated_world_origin,
    blink::WebSocketHandshakeThrottle::OnCompletion completion_callback) {
  DCHECK(!completion_callback_);
  completion_callback_ = std::move(completion_callback);
  url_ = url;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  MaybeSendExtensionWebRequestData(url, creator_origin, isolated_world_origin);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // TODO(crbug.com/40934395) [Also TODO(thefrog)]: Remove histogram.
  scheme_logger::LogScheme(url, "SafeBrowsing.WebSocketCheck.UrlScheme");
  // TODO(crbug.com/40934395) [Also TODO(thefrog)]: Remove histogram.
  base::UmaHistogramBoolean("SafeBrowsing.WebSocketCheck.Skipped", true);
  std::move(completion_callback_).Run(std::nullopt);
  // |this| is destroyed here.
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void WebSocketSBHandshakeThrottle::MaybeSendExtensionWebRequestData(
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
#endif

}  // namespace safe_browsing
