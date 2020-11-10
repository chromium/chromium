// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
class WebURLRequest;
}  // namespace blink

namespace previews {
class ResourceLoadingHintsAgent;
}  // namespace previews

namespace subresource_redirect {

class SubresourceRedirectHintsAgent;

// This class handles internal redirects for subresouces on HTTPS sites to
// compressed versions of subresources.
class SubresourceRedirectURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
  MaybeCreateThrottle(const blink::WebURLRequest& request,
                      int render_frame_id);

  ~SubresourceRedirectURLLoaderThrottle() override;

  previews::ResourceLoadingHintsAgent* GetResourceLoadingHintsAgent();

  // virtual for testing.
  virtual SubresourceRedirectHintsAgent* GetSubresourceRedirectHintsAgent();

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

 private:
  friend class TestSubresourceRedirectURLLoaderThrottle;

  SubresourceRedirectURLLoaderThrottle(int render_frame_id,
                                       bool allowed_to_redirect);

  // Callback invoked when the redirect fetch times out.
  void OnRedirectTimeout();

  // Render frame id to get the hints agent of the render frame.
  const int render_frame_id_;

  // Whether the subresource can be redirected or not and what was the reason if
  // its not eligible.
  SubresourceRedirectHintsAgent::RedirectResult redirect_result_;

  // Whether this resource was actually redirected to compressed server origin.
  // This will be true when the redirect was attempted. Will be false when
  // redirect failed due to neterrors, or redirect was not attempted (but
  // coverage metrics recorded), or redirect was not needed when the initial URL
  // itself is compressed origin.
  bool did_redirect_compressed_origin_ = false;

  // Timer to detect whether the response from compression server has timed out.
  std::unique_ptr<base::OneShotTimer> redirect_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceRedirectURLLoaderThrottle);
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
