// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/renderer/subresource_redirect/login_robots_compression_metrics.h"
#include "chrome/renderer/subresource_redirect/public_resource_decider_agent.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
class WebURLRequest;
}  // namespace blink

namespace subresource_redirect {

// This class handles internal redirects for HTTPS public subresources
// (currently only for images) compressed versions of subresources. When the
// redirect fails/timesout the original image is fetched directly. Subclasses
// should implement the decider logic if an URL should be compressed.
class SubresourceRedirectURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
  MaybeCreateThrottle(const blink::WebURLRequest& request, int render_frame_id);

  SubresourceRedirectURLLoaderThrottle(int render_frame_id,
                                       bool allowed_to_redirect);
  ~SubresourceRedirectURLLoaderThrottle() override;

  SubresourceRedirectURLLoaderThrottle(
      const SubresourceRedirectURLLoaderThrottle&) = delete;
  SubresourceRedirectURLLoaderThrottle& operator=(
      const SubresourceRedirectURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  const char* NameForLoggingWillStartRequest() override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  void WillOnCompleteWithError(const network::URLLoaderCompletionStatus& status,
                               bool* defer) override;
  // Overridden to do nothing as the default implementation is NOT_REACHED()
  void DetachFromCurrentSequence() override;

 private:
  friend class SubresourceRedirectPublicImageHintsDeciderAgentTest;

  // Callback to notify the decision of decider subclasses.
  void NotifyRedirectDeciderDecision(SubresourceRedirectResult);

  // Start the timer for redirect fetch timeout.
  void StartRedirectTimeoutTimer();

  // Callback invoked when the redirect fetch times out.
  void OnRedirectTimeout();

  // Render frame id to get the hints agent of the render frame.
  const int render_frame_id_;

  // The current state of redirect.
  PublicResourceDeciderRedirectState redirect_state_ =
      PublicResourceDeciderRedirectState::kNone;

  // Timer to detect whether the response from compression server has timed out.
  std::unique_ptr<base::OneShotTimer> redirect_timeout_timer_;

  // Whether the subresource can be redirected or not and what was the reason if
  // its not eligible.
  SubresourceRedirectResult redirect_result_ =
      SubresourceRedirectResult::kUnknown;

  // Used to record the image load and compression metrics.
  base::Optional<LoginRobotsCompressionMetrics>
      login_robots_compression_metrics_;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<SubresourceRedirectURLLoaderThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
