// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/renderer/websocket_sb_handshake_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/resource_type.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace safe_browsing {

WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : render_frame_id_(render_frame_id),
      safe_browsing_(safe_browsing),
      result_(Result::UNKNOWN) {}

WebSocketSBHandshakeThrottle::~WebSocketSBHandshakeThrottle() {
  // ThrottleHandshake() should always be called, but since that is done all the
  // way over in Blink, just avoid logging if it is not called rather than
  // DCHECK()ing.
  if (start_time_.is_null())
    return;
  if (result_ == Result::UNKNOWN) {
    result_ = Result::ABANDONED;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Abandoned",
                        base::TimeTicks::Now() - start_time_);
  }
}

void WebSocketSBHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    blink::WebSocketHandshakeThrottle::OnCompletion completion_callback) {
  DCHECK(!url_checker_);
  DCHECK(!completion_callback_);
  completion_callback_ = std::move(completion_callback);
  url_ = url;
  int load_flags = 0;
  start_time_ = base::TimeTicks::Now();
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, url_checker_.BindNewPipeAndPassReceiver(), url, "GET",
      net::HttpRequestHeaders(), load_flags,
      content::ResourceType::kSubResource, false /* has_user_gesture */,
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
  DCHECK(!start_time_.is_null());
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  if (proceed) {
    result_ = Result::SAFE;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Safe", elapsed);
    std::move(completion_callback_).Run(base::nullopt);
  } else {
    // When the insterstitial is dismissed the page is navigated and this object
    // is destroyed before reaching here.
    result_ = Result::BLOCKED;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Blocked", elapsed);
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

  // TODO(yzshen): Notify the network service to pause processing response body.
  if (!notifier_receiver_) {
    notifier_receiver_ =
        std::make_unique<mojo::Receiver<mojom::UrlCheckNotifier>>(this);
  }
  notifier_receiver_->Bind(std::move(slow_check_notifier));
}

void WebSocketSBHandshakeThrottle::OnMojoDisconnect() {
  DCHECK_EQ(result_, Result::UNKNOWN);

  url_checker_.reset();
  notifier_receiver_.reset();

  // Make the destructor record NOT_SUPPORTED in the result histogram.
  result_ = Result::NOT_SUPPORTED;
  // Don't record the time elapsed because it's unlikely to be meaningful.
  std::move(completion_callback_).Run(base::nullopt);
  // |this| is destroyed here.
}

}  // namespace safe_browsing
