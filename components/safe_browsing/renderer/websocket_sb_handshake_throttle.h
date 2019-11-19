// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of SafeBrowsing for WebSockets. This code runs inside the
// render process, calling the interface defined in safe_browsing.mojom to
// communicate with the SafeBrowsing service.

#ifndef COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

class WebSocketSBHandshakeThrottle : public blink::WebSocketHandshakeThrottle,
                                     public mojom::UrlCheckNotifier {
 public:
  WebSocketSBHandshakeThrottle(mojom::SafeBrowsing* safe_browsing,
                               int render_frame_id);
  ~WebSocketSBHandshakeThrottle() override;

  void ThrottleHandshake(const blink::WebURL& url,
                         blink::WebSocketHandshakeThrottle::OnCompletion
                             completion_callback) override;

 private:
  // These values are logged to UMA so do not renumber or reuse.
  enum class Result {
    UNKNOWN = 0,
    SAFE = 1,
    BLOCKED = 2,
    ABANDONED = 3,
    NOT_SUPPORTED = 4,
    RESULT_COUNT
  };

  // mojom::UrlCheckNotifier implementation.
  void OnCompleteCheck(bool proceed, bool showed_interstitial) override;

  void OnCheckResult(
      mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
      bool proceed,
      bool showed_interstitial);
  void OnMojoDisconnect();

  const int render_frame_id_;
  GURL url_;
  blink::WebSocketHandshakeThrottle::OnCompletion completion_callback_;
  mojo::Remote<mojom::SafeBrowsingUrlChecker> url_checker_;
  mojom::SafeBrowsing* safe_browsing_;
  std::unique_ptr<mojo::Receiver<mojom::UrlCheckNotifier>> notifier_receiver_;
  base::TimeTicks start_time_;
  Result result_;

  base::WeakPtrFactory<WebSocketSBHandshakeThrottle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebSocketSBHandshakeThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
