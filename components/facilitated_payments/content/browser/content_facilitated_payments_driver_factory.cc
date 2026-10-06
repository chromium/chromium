// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace payments::facilitated {

namespace {

// Returns whether `signals` meets the rule to start extracting images.
bool ShouldExtractImagesForQrCode(const mojom::HeuristicSignals& signals) {
  // Pages with a facilitated payment link are handled by the payment link
  // flow, so offering a QR code payment as well would duplicate the prompt.
  if (signals.has_facilitated_payment_link) {
    return false;
  }
  // A square image alone is not enough (logos, avatars or icons), so a keyword
  // is also required to suggest that the page is about paying.
  return signals.has_square_candidate &&
         (signals.url_keyword_match || signals.text_keyword_match);
}

}  // namespace

ContentFacilitatedPaymentsDriverFactory::
    ContentFacilitatedPaymentsDriverFactory(content::WebContents* web_contents,
                                            FacilitatedPaymentsClient* client)
    : content::WebContentsObserver(web_contents),
      client_(CHECK_DEREF(client)) {}

ContentFacilitatedPaymentsDriverFactory::
    ~ContentFacilitatedPaymentsDriverFactory() = default;

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
      &*client_, render_frame_host, std::make_unique<SecurityChecker>(),
      /*factory=*/this);
  DCHECK_EQ(driver_map_.find(render_frame_host)->second.get(), driver.get());
  return *iter->second;
}

void ContentFacilitatedPaymentsDriverFactory::OnHeuristicSignalsReported(
    content::RenderFrameHost* render_frame_host,
    const mojom::HeuristicSignals& signals) {
  // A report can arrive after the page has been navigated away from, so it
  // must not trigger extraction on a frame that is no longer shown.
  if (!render_frame_host->IsActive() ||
      !render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  if (has_detected_qr_code_ || is_evaluating_qr_code_) {
    return;
  }
  if (!ShouldExtractImagesForQrCode(signals)) {
    return;
  }

  is_evaluating_qr_code_ = true;
  // TODO(crbug.com/556832672): Extract images from `render_frame_host` and
  // decode them, then clear `is_evaluating_qr_code_` and set
  // `has_detected_qr_code_` from the result.
}

void ContentFacilitatedPaymentsDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  driver_map_.erase(render_frame_host);
}

void ContentFacilitatedPaymentsDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->IsInOutermostMainFrame()) {
    return;
  }
  is_evaluating_qr_code_ = false;
  has_detected_qr_code_ = false;

  auto& driver = GetOrCreateForFrame(navigation_handle->GetRenderFrameHost());
#if BUILDFLAG(IS_ANDROID)
  driver.DidNavigateToOrAwayFromPage();
#endif  // BUILDFLAG(IS_ANDROID)

  // The agent starts every document dormant, so the decision is pushed on each
  // commit rather than only when detection is eligible.
  const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>& agent =
      driver.GetFacilitatedPaymentsAgent();
  if (!agent.is_bound()) {
    return;
  }
  agent->SetQrCodeDetectionEnabled(
      IsEligibleForQrCodeDetection(navigation_handle->GetURL()));
}

#if BUILDFLAG(IS_ANDROID)
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

void ContentFacilitatedPaymentsDriverFactory::OnTextCopiedToClipboard(
    content::RenderFrameHost* render_frame_host,
    const std::u16string& copied_text) {
  content::RenderFrameHost* main_frame =
      render_frame_host->GetOutermostMainFrame();

  // If the copy event occurred in iframe, only proceed if the iframe flag is
  // enabled.
  if (render_frame_host != main_frame &&
      !base::FeatureList::IsEnabled(kEnableIframeForPix)) {
    LogPixFlowExitedReason(PixFlowExitedReason::kPixCodeInIFrame);
    return;
  }

  if (!render_frame_host->IsActive()) {
    LogPixFlowExitedReason(PixFlowExitedReason::kFrameNotActive);
    return;
  }

  std::optional<GURL> iframe_url;

  bool is_same_origin = false;
  if (render_frame_host != main_frame) {
    if (render_frame_host->IsErrorDocument()) {
      LogPixFlowExitedReason(PixFlowExitedReason::kFrameIsErrorDocument);
      return;
    }
    // If the copy event occurred in an iframe, capture the iframe URL.
    iframe_url = render_frame_host->GetLastCommittedURL();
    is_same_origin =
        render_frame_host->GetLastCommittedOrigin().IsSameOriginWith(
            main_frame->GetLastCommittedOrigin());
  }

  auto& driver = GetOrCreateForFrame(render_frame_host);

  // Pass the main frame URL as the primary identifier for the merchant site,
  // while providing the optional iframe URL for PSP allowlist verification.
  // To ensure that the PixManager receives the main frame origin for account
  // linking, the third parameter is now always the main frame origin.
  driver.OnTextCopiedToClipboard(
      /*main_frame_url=*/main_frame->GetLastCommittedURL(),
      /*iframe_url=*/iframe_url,
      /*main_frame_origin=*/main_frame->GetLastCommittedOrigin(), copied_text,
      render_frame_host->GetPageUkmSourceId(),
      /*is_same_origin=*/is_same_origin);
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ContentFacilitatedPaymentsDriverFactory::IsEligibleForQrCodeDetection(
    const GURL& url) const {
  if (!base::FeatureList::IsEnabled(kEnableDesktopQrCodeDetection)) {
    return false;
  }

  optimization_guide::OptimizationGuideDecider* decider =
      client_->GetOptimizationGuideDecider();
  if (!decider) {
    return false;
  }

  // The Optimization Guide list answers "can this site be optimized?", so
  // `kTrue` means `url` is allowed. `kUnknown` is returned when the
  // optimization type has not been registered yet, and is treated as a
  // rejection.
  return decider->CanApplyOptimization(
             url,
             optimization_guide::proto::
                 PAYMENT_QR_CODE_MERCHANT_URL_REGEX_ALLOWLIST,
             /*optimization_metadata=*/nullptr) ==
         optimization_guide::OptimizationGuideDecision::kTrue;
}

}  // namespace payments::facilitated
