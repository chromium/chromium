// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"

namespace payments::facilitated {

ContentFacilitatedPaymentsDriverFactory::
    ContentFacilitatedPaymentsDriverFactory(
        content::WebContents* web_contents,
        optimization_guide::OptimizationGuideDecider*
            optimization_guide_decider)
    : content::WebContentsUserData<ContentFacilitatedPaymentsDriverFactory>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      optimization_guide_decider_(optimization_guide_decider) {}

ContentFacilitatedPaymentsDriverFactory::
    ~ContentFacilitatedPaymentsDriverFactory() {
  DCHECK(driver_map_.empty());
}

void ContentFacilitatedPaymentsDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  driver_map_.erase(render_frame_host);
}

void ContentFacilitatedPaymentsDriverFactory::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // The driver is only created for the outermost main frame as the PIX code is
  // only expected to be present there. Only active frames allowed.
  if (render_frame_host != render_frame_host->GetOutermostMainFrame() ||
      !render_frame_host->IsActive()) {
    return;
  }
  auto& driver = GetOrCreateForFrame(render_frame_host);
  // Initialize PIX code detection.
  driver.DidFinishLoad(validated_url);
}

ContentFacilitatedPaymentsDriver&
ContentFacilitatedPaymentsDriverFactory::GetOrCreateForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto [iter, insertion_happened] =
      driver_map_.emplace(render_frame_host, nullptr);
  std::unique_ptr<ContentFacilitatedPaymentsDriver>& driver = iter->second;
  if (!insertion_happened) {
    DCHECK(driver);
    return *iter->second;
  }
  driver = std::make_unique<ContentFacilitatedPaymentsDriver>(
      optimization_guide_decider_, render_frame_host);
  DCHECK_EQ(driver_map_.find(render_frame_host)->second.get(), driver.get());
  return *iter->second;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentFacilitatedPaymentsDriverFactory);

}  // namespace payments::facilitated
