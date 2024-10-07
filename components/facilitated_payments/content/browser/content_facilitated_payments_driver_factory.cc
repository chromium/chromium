// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"

#include "base/check_deref.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/features/features.h"
#include "content/public/browser/navigation_handle.h"

namespace payments::facilitated {

ContentFacilitatedPaymentsDriverFactory::
    ContentFacilitatedPaymentsDriverFactory(
        content::WebContents* web_contents,
        FacilitatedPaymentsClient* client,
        optimization_guide::OptimizationGuideDecider*
            optimization_guide_decider)
    : content::WebContentsObserver(web_contents),
      client_(CHECK_DEREF(client)),
      optimization_guide_decider_(optimization_guide_decider) {}

ContentFacilitatedPaymentsDriverFactory::
    ~ContentFacilitatedPaymentsDriverFactory() {
  DCHECK(driver_map_.empty());
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
      &*client_, optimization_guide_decider_, render_frame_host,
      std::make_unique<SecurityChecker>());
  DCHECK_EQ(driver_map_.find(render_frame_host)->second.get(), driver.get());
  return *iter->second;
}

void ContentFacilitatedPaymentsDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  driver_map_.erase(render_frame_host);
}

void ContentFacilitatedPaymentsDriverFactory::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  // All facilitated payments processes are run only on the outermost main
  // frame.
  if (render_frame_host != render_frame_host->GetOutermostMainFrame()) {
    return;
  }
  // User visible pages are active i.e. `LifecycleState == kActive`. A
  // RenderFrameHost state change where `old_state == kActive` represents a
  // navigation away from an active page. When navigating away, all facilitated
  // payments processes should be abandoned.
  if (old_state != content::RenderFrameHost::LifecycleState::kActive) {
    return;
  }
  if (auto iter = driver_map_.find(render_frame_host);
      iter != driver_map_.end()) {
    iter->second->DidNavigateToOrAwayFromPage();
  }
}

void ContentFacilitatedPaymentsDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->IsInOutermostMainFrame()) {
    return;
  }
  auto& driver = GetOrCreateForFrame(navigation_handle->GetRenderFrameHost());
  driver.DidNavigateToOrAwayFromPage();
}

void ContentFacilitatedPaymentsDriverFactory::OnTextCopiedToClipboard(
    content::RenderFrameHost* render_frame_host,
    const std::u16string& copied_text) {
  // The Facilitated Payments infra is initiated for both Pix and eWallet,
  // however the Pix payflow should only be initiated if its flag is enabled.
  if (!base::FeatureList::IsEnabled(kEnablePixPayments)) {
    return;
  }

  if (render_frame_host != render_frame_host->GetOutermostMainFrame() ||
      !render_frame_host->IsActive()) {
    return;
  }

  auto& driver = GetOrCreateForFrame(render_frame_host);

  driver.OnTextCopiedToClipboard(render_frame_host->GetLastCommittedURL(),
                                 copied_text,
                                 render_frame_host->GetPageUkmSourceId());
}

}  // namespace payments::facilitated
