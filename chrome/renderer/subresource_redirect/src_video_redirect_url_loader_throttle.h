// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SRC_VIDEO_REDIRECT_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SRC_VIDEO_REDIRECT_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/subresource_redirect/public_resource_decider_agent.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
class WebURLRequest;
}  // namespace blink

namespace subresource_redirect {

// This class records coverage information for src videos. Whether the video is
// eligible for compression, the reasons for ineligibility, and the total bytes
// of the video are recorded.
class SrcVideoRedirectURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<SrcVideoRedirectURLLoaderThrottle> MaybeCreateThrottle(
      const blink::WebURLRequest& request,
      int render_frame_id);

  explicit SrcVideoRedirectURLLoaderThrottle(int render_frame_id);
  ~SrcVideoRedirectURLLoaderThrottle() override;

  SrcVideoRedirectURLLoaderThrottle(const SrcVideoRedirectURLLoaderThrottle&) =
      delete;
  SrcVideoRedirectURLLoaderThrottle& operator=(
      const SrcVideoRedirectURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  // Overridden to do nothing as the default implementation is NOT_REACHED()
  void DetachFromCurrentSequence() override;

 private:
  // Callback to notify the decision of the decider.
  void NotifyRedirectDeciderDecision(SubresourceRedirectResult redirect_result);

  // Render frame id to get the hints agent of the render frame.
  const int render_frame_id_;

  // The current state of redirect.
  PublicResourceDeciderRedirectState redirect_state_ =
      PublicResourceDeciderRedirectState::kNone;

  // Whether the subresource can be redirected or not and what was the reason if
  // its not eligible.
  SubresourceRedirectResult redirect_result_ =
      SubresourceRedirectResult::kUnknown;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<SrcVideoRedirectURLLoaderThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SRC_VIDEO_REDIRECT_URL_LOADER_THROTTLE_H_
