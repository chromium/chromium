// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_DECIDER_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_DECIDER_AGENT_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/renderer/subresource_redirect/public_resource_decider_agent.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "url/gurl.h"

namespace subresource_redirect {

// The decider agent implementation that allows image compression based on
// public image hints received. Only redirects public images for mainframes.
class PublicImageHintsDeciderAgent : public PublicResourceDeciderAgent {
 public:
  PublicImageHintsDeciderAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~PublicImageHintsDeciderAgent() override;

  PublicImageHintsDeciderAgent(const PublicImageHintsDeciderAgent&) = delete;
  PublicImageHintsDeciderAgent& operator=(const PublicImageHintsDeciderAgent&) =
      delete;

 private:
  friend class SubresourceRedirectPublicImageHintsDeciderAgentTest;

  // content::RenderFrameObserver:
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void OnDestruct() override;

  // mojom::SubresourceRedirectHintsReceiver:
  void SetCompressPublicImagesHints(
      mojom::CompressPublicImagesHintsPtr images_hints) override;
  void SetLoggedInState(bool is_logged_in) override;

  // PublicResourceDeciderAgent:
  base::Optional<SubresourceRedirectResult> ShouldRedirectSubresource(
      const GURL& url,
      ShouldRedirectDecisionCallback callback) override;
  void RecordMetricsOnLoadFinished(
      const GURL& url,
      int64_t content_length,
      SubresourceRedirectResult redirect_result) override;
  void NotifyCompressedResourceFetchFailed(
      base::TimeDelta retry_after) override;

  bool IsMainFrame() const;

  // Clears the image hint urls.
  void ClearImageHints();

  // Called when the hint receive timer expires to flush the metrics for
  // resources that no hint has been received and clears
  // |unavailable_image_hints_urls_|.
  void OnHintsReceiveTimeout();

  // Records the breakdown of bytes to UKM metrics.
  void RecordMetrics(int64_t content_length,
                     SubresourceRedirectResult redirect_result) const;

  // Record the breakdown of bytes for unavailable image hints, whether the
  // hints fetch timed out, the image was not in the delayed hints, or the
  // image was in the delayed hints.
  void RecordImageHintsUnavailableMetrics();

  // The raw spec of image urls that are determined public, received from image
  // hints. Will be base::nullopt after the navigation starts and until the
  // hints have been received.
  base::Optional<base::flat_set<std::string>> public_image_urls_;

  // To trigger the timeout for the hints to be received from the time
  // navigation starts.
  base::OneShotTimer hint_receive_timeout_timer_;

  // The urls and resource size of images that were not redirectable due to
  // image hints were not unavailable at the time of image fetch. This list is
  // kept until hints are received or hints retrieval times out. This is used
  // for metrics purposes.
  base::flat_set<std::pair<std::string, int64_t>> unavailable_image_hints_urls_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_DECIDER_AGENT_H_
