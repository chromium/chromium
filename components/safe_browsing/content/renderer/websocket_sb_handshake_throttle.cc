// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/websocket_sb_handshake_throttle.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace safe_browsing {

WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : render_frame_id_(render_frame_id), safe_browsing_(safe_browsing) {}

#if BUILDFLAG(ENABLE_EXTENSIONS)
WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id,
    mojom::ExtensionWebRequestReporter* extension_web_request_reporter)
    : render_frame_id_(render_frame_id),
      safe_browsing_(safe_browsing),
      extension_web_request_reporter_(
          std::move(extension_web_request_reporter)) {}
#endif

WebSocketSBHandshakeThrottle::~WebSocketSBHandshakeThrottle() = default;

void WebSocketSBHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    blink::WebSocketHandshakeThrottle::OnCompletion completion_callback) {
  DCHECK(!url_checker_);
  DCHECK(!completion_callback_);
  completion_callback_ = std::move(completion_callback);
  url_ = url;
  int load_flags = 0;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Send web request data to the browser if destination is WS/WSS scheme.
  if (creator_origin.Protocol().Ascii() == extensions::kExtensionScheme &&
      url_.SchemeIsWSOrWSS()) {
    const std::string& origin_extension_id =
        creator_origin.Host().Utf8().data();
    extension_web_request_reporter_->SendWebRequestData(
        origin_extension_id, url, mojom::WebRequestProtocolType::kWebSocket);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  DCHECK_EQ(state_, State::kInitial);
  state_ = State::kStarted;
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, url_checker_.BindNewPipeAndPassReceiver(), url, "GET",
      net::HttpRequestHeaders(), load_flags,
      network::mojom::RequestDestination::kEmpty, false /* has_user_gesture */,
      false /* originated_from_service_worker */,
      base::BindOnce(&WebSocketSBHandshakeThrottle::OnCheckResult,
                     weak_factory_.GetWeakPtr()));

  // This use of base::Unretained() is safe because the handler will not be
  // called after |url_checker_| is destroyed, and it is owned by this object.
  url_checker_.set_disconnect_handler(base::BindOnce(
      &WebSocketSBHandshakeThrottle::OnMojoDisconnect, base::Unretained(this)));
}

void WebSocketSBHandshakeThrottle::OnCompleteCheck(bool proceed,
                                                   bool showed_interstitial) {
  DCHECK_EQ(state_, State::kStarted);
  if (proceed) {
    state_ = State::kSafe;
    std::move(completion_callback_).Run(absl::nullopt);
  } else {
    // When the insterstitial is dismissed the page is navigated and this object
    // is destroyed before reaching here.
    state_ = State::kBlocked;
    std::move(completion_callback_)
        .Run(blink::WebString::FromUTF8(base::StringPrintf(
            "WebSocket connection to %s failed safe browsing check",
            url_.spec().c_str())));
  }
  // |this| is destroyed here.
}

void WebSocketSBHandshakeThrottle::OnCheckResult(
    mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
    bool proceed,
    bool showed_interstitial) {
  if (!slow_check_notifier.is_valid()) {
    OnCompleteCheck(proceed, showed_interstitial);
    return;
  }

  // TODO(yzshen): Notify the network service to stop reading from the
  // WebSocket.
  if (!notifier_receiver_) {
    notifier_receiver_ =
        std::make_unique<mojo::Receiver<mojom::UrlCheckNotifier>>(this);
  }
  notifier_receiver_->Bind(std::move(slow_check_notifier));
}

void WebSocketSBHandshakeThrottle::OnMojoDisconnect() {
  DCHECK(state_ == State::kStarted);

  url_checker_.reset();
  notifier_receiver_.reset();

  state_ = State::kNotSupported;
  std::move(completion_callback_).Run(absl::nullopt);
  // |this| is destroyed here.
}

}  // namespace safe_browsing
