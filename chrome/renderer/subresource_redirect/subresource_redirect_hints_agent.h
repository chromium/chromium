// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/common/subresource_redirect_service.mojom.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Holds the public image URL hints to be queried by URL loader throttles. Only
// created for mainframes.
class SubresourceRedirectHintsAgent {
 public:
  enum class RedirectResult {
    // The image was found in the image hints and is eligible to be redirected
    // to compressed version.
    kRedirectable,

    // Possible reasons for ineligibility.
    // because the image hint list was not retrieved within certain time limit
    // of navigation start,
    kIneligibleImageHintsUnavailable,

    // because the image hint list was not retrieved at the time of image fetch,
    // but the image URL was found in the hint list, which finished fetching
    // later.
    kIneligibleImageHintsUnavailableButRedirectableBytes,

    // because the image hint list was not retrieved at the time of image fetch,
    // and the image URL was not in the hint list as well, which finished
    // fetching later.
    kIneligibleImageHintsUnavailableAndMissingInHintsBytes,

    // because the image URL was not found in the image hints.
    kIneligibleMissingInImageHints,

    // because of other reasons such as subframe images, Blink did not allow the
    // redirect due to non <img> element, security limitations, javascript
    // initiated image, etc.
    kIneligibleOtherImage
  };

  SubresourceRedirectHintsAgent();
  ~SubresourceRedirectHintsAgent();

  SubresourceRedirectHintsAgent(const SubresourceRedirectHintsAgent&) = delete;
  SubresourceRedirectHintsAgent& operator=(
      const SubresourceRedirectHintsAgent&) = delete;

  // Called when a navigation starts to clear the state from previous
  // navigation.
  void DidStartNavigation();
  void ReadyToCommitNavigation(int render_frame_id);

  void SetCompressPublicImagesHints(
      blink::mojom::CompressPublicImagesHintsPtr images_hints);

  RedirectResult ShouldRedirectImage(const GURL& url) const;

  // Record metrics when the resource load is finished.
  void RecordMetricsOnLoadFinished(const GURL& url,
                                   int64_t content_length,
                                   RedirectResult redirect_result);

  // Clears the image hint urls.
  void ClearImageHints();

 private:
  void OnHintsReceiveTimeout();
  void RecordMetrics(int64_t content_length,
                     RedirectResult redirect_result) const;

  // Record the breakdown of bytes for unavailable image hints, whether the
  // hints fetch timed out, or the image was not in the delayed hints, or the
  // image was in the delayed hints.
  void RecordImageHintsUnavailableMetrics();

  bool public_image_urls_received_ = false;
  base::flat_set<std::string> public_image_urls_;

  // To trigger the timeout for the hints to be received from the time
  // navigation starts.
  base::OneShotTimer hint_receive_timeout_timer_;

  // The urls and resource size of images that were not redirectable due to
  // image hints was unavailable at the time of image fetch. This list is kept
  // until hints are received or it times out and used to record metrics.
  base::flat_set<std::pair<std::string, int64_t>> unavailable_image_hints_urls_;

  // ID of the current render frame (will be main frame). Populated when the
  // navigation commits. Used to record ukm against.
  int render_frame_id_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_
