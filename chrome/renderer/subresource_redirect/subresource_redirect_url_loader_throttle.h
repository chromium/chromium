// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
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
  using RedirectDecisionCallback = base::OnceCallback<void(bool)>;

  static std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
  MaybeCreateThrottle(const blink::WebURLRequest& request, int render_frame_id);

  explicit SubresourceRedirectURLLoaderThrottle(int render_frame_id);
  ~SubresourceRedirectURLLoaderThrottle() override;

  SubresourceRedirectURLLoaderThrottle(
      const SubresourceRedirectURLLoaderThrottle&) = delete;
  SubresourceRedirectURLLoaderThrottle& operator=(
      const SubresourceRedirectURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
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

  // Determine whether the image url should be redirected. When the
  // determination can be made immediately, the decision should be returned.
  // Otherwise base::nullopt should be returned and the callback should be
  // invoked with the decision asynchronously.
  virtual base::Optional<bool> ShouldRedirectImage(
      const GURL& url,
      RedirectDecisionCallback callback) = 0;

  // Indicates the subresource redirect failed, and the image will be fetched
  // directly from the  origin instead. The failures can be due to non-2xx
  // http responses or other net errors
  virtual void OnRedirectedLoadCompleteWithError() = 0;

  // Notifies the image load finished.
  virtual void RecordMetricsOnLoadFinished(const GURL& url,
                                           int64_t content_length) = 0;

  content::RenderFrame* GetRenderFrame() const {
    return content::RenderFrame::FromRoutingID(render_frame_id_);
  }

 private:
  // Different states the subresource redirection can be in.
  enum class RedirectState {
    kNone,

    // The redirect decision is pending from the underlying decider.
    kDeciderDecisionPending,

    // Redirect was disallowed by the underlying decider e.g., robots rules
    // decider.
    kDeciderDisallowed,

    // The decider allowed redirect, and was attempted.
    kRedirectAttempted,

    // Failed due to http response codes, net errors, and the subresource was
    // fetched from original origin.
    kRedirectFailed
  };

  friend class TestPublicImageHintsURLLoaderThrottle;

  // Callback to notify the decision of decider subclasses.
  void NotifyRedirectDeciderDecision(bool is_allowed);

  // Start the timer for redirect fetch timeout.
  void StartRedirectTimeoutTimer();

  // Callback invoked when the redirect fetch times out.
  void OnRedirectTimeout();

  // Render frame id to get the hints agent of the render frame.
  const int render_frame_id_;

  // The current state of redirect.
  RedirectState redirect_state_ = RedirectState::kNone;

  // Timer to detect whether the response from compression server has timed out.
  std::unique_ptr<base::OneShotTimer> redirect_timeout_timer_;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<SubresourceRedirectURLLoaderThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
