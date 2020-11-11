// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace subresource_redirect {

// This class handles internal redirects for public subresouces on HTTPS sites
// to compressed versions of subresources.
class PublicImageHintsURLLoaderThrottle
    : public SubresourceRedirectURLLoaderThrottle {
 public:
  PublicImageHintsURLLoaderThrottle(int render_frame_id,
                                    bool allowed_to_redirect);

  ~PublicImageHintsURLLoaderThrottle() override;

  // SubresourceRedirectURLLoaderThrottle:
  bool ShouldRedirectImage(const GURL& url) override;
  void OnRedirectedLoadCompleteWithError() override;
  void RecordMetricsOnLoadFinished(const GURL& url,
                                   int64_t content_length) override;

 private:
  friend class TestPublicImageHintsURLLoaderThrottle;

  SubresourceRedirectHintsAgent* GetSubresourceRedirectHintsAgent();

  // Whether the subresource can be redirected or not and what was the reason if
  // its not eligible.
  SubresourceRedirectHintsAgent::RedirectResult redirect_result_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_IMAGE_HINTS_URL_LOADER_THROTTLE_H_
