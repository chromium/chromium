// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/facilitated_payments/core/features/features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace payments::facilitated {
namespace {

// A PSP-supplied marker for pages handled by the payment link flow.
constexpr std::string_view kFacilitatedPaymentLinkSelector =
    "head link[rel=\"facilitated-payment\"]";
constexpr std::string_view kCandidateImageElementsSelector = "img,canvas";
// The page summary, scanned in addition to the title (read separately via
// `WebDocument::Title()`).
constexpr std::string_view kMetaDescriptionSelector =
    "meta[name=\"description\"]";
constexpr std::string_view kContentAttribute = "content";
constexpr std::string_view kHeuristicsLatencyHistogram =
    "FacilitatedPayments.QrCode.Heuristics.Latency";

// Splits the comma-separated Finch keyword list. Parsed per scan rather than
// cached so that tests can vary the parameter between scans.
std::vector<std::string> GetKeywords() {
  return base::SplitString(kQrCodeDetectionKeywords.Get(), ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

bool ContainsAnyKeyword(std::string_view haystack,
                        const std::vector<std::string>& keywords) {
  const std::string lowered = base::ToLowerASCII(haystack);
  return std::ranges::any_of(keywords, [&](const std::string& keyword) {
    return lowered.find(keyword) != std::string::npos;
  });
}

bool HasFacilitatedPaymentLink(const blink::WebDocument& document) {
  return !document
              .QuerySelector(
                  blink::WebString::FromAscii(kFacilitatedPaymentLinkSelector))
              .IsNull();
}

// Returns whether the document renders an `<img>` or `<canvas>` whose layout
// box is roughly square and large enough to be a payment QR code.
bool HasQrCodeImageCandidate(const blink::WebDocument& document) {
  const int min_image_size = kQrCodeDetectionMinImageSize.Get();
  const double max_aspect_ratio = kQrCodeDetectionMaxAspectRatio.Get();
  for (const blink::WebElement& element : document.QuerySelectorAll(
           blink::WebString::FromAscii(kCandidateImageElementsSelector))) {
    const gfx::Rect bounds = element.BoundsInWidget();
    const int min_side = std::min(bounds.width(), bounds.height());
    const int max_side = std::max(bounds.width(), bounds.height());
    if (min_side >= min_image_size && max_side <= min_side * max_aspect_ratio) {
      return true;
    }
  }
  return false;
}

bool UrlMatchesKeyword(const blink::WebDocument& document,
                       const std::vector<std::string>& keywords) {
  const GURL url = document.Url();
  return ContainsAnyKeyword(url.path(), keywords) ||
         ContainsAnyKeyword(url.query(), keywords);
}

// Checks the document title and `<meta name="description">`. Full-body text is
// deliberately not scanned, to keep the scan inside a single frame budget.
bool TextMatchesKeyword(const blink::WebDocument& document,
                        const std::vector<std::string>& keywords) {
  if (ContainsAnyKeyword(document.Title().Utf8(), keywords)) {
    return true;
  }
  const blink::WebElement meta = document.QuerySelector(
      blink::WebString::FromAscii(kMetaDescriptionSelector));
  if (meta.IsNull()) {
    return false;
  }
  return ContainsAnyKeyword(
      meta.GetAttribute(blink::WebString::FromAscii(kContentAttribute)).Utf8(),
      keywords);
}

}  // namespace

FacilitatedPaymentsAgent::FacilitatedPaymentsAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame ? render_frame->GetWebFrame()
                                                : nullptr) {
  if (registry) {
    registry->AddInterface<mojom::FacilitatedPaymentsAgent>(
        base::BindRepeating(&FacilitatedPaymentsAgent::BindPendingReceiver,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

FacilitatedPaymentsAgent::~FacilitatedPaymentsAgent() = default;

void FacilitatedPaymentsAgent::SetQrCodeDetectionEnabled(bool enabled) {
  is_qr_code_detection_enabled_ = enabled;
  if (!is_qr_code_detection_enabled_) {
    rescan_timer_.Stop();
    follow_up_rescan_timer_.Stop();
  }
}

void FacilitatedPaymentsAgent::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  OnTriggeredRescan();
}

void FacilitatedPaymentsAgent::DidChangeScrollOffset(blink::mojom::ScrollType) {
  OnTriggeredRescan();
}

void FacilitatedPaymentsAgent::DidFinishLoad() {
  OnTriggeredRescan();
  if (!is_qr_code_detection_enabled_) {
    return;
  }
  // Payment QR codes are often rendered after the page loads, e.g. once an
  // order is created, with no event that triggers a rescan, so a few rescans
  // follow every load.
  follow_up_rescan_count_ = 0;
  follow_up_rescan_timer_.Start(
      FROM_HERE, kFollowUpRescanInterval,
      base::BindRepeating(&FacilitatedPaymentsAgent::OnFollowUpRescan,
                          weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsAgent::OnFollowUpRescan() {
  if (++follow_up_rescan_count_ >= kMaxFollowUpRescans) {
    follow_up_rescan_timer_.Stop();
  }
  // A pending debounced scan already covers this rescan.
  if (!is_qr_code_detection_enabled_ || rescan_timer_.IsRunning()) {
    return;
  }
  // Each report can start a decoding pass, and a page has only a few, so
  // unchanged signals are not reported again; otherwise, e.g. a square logo
  // would use up the passes before a QR code is rendered.
  mojom::HeuristicSignalsPtr signals = ScanSignals();
  if (signals == last_reported_signals_) {
    return;
  }
  ReportSignals(std::move(signals));
}

void FacilitatedPaymentsAgent::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // A new document starts dormant until the browser opts it in, so that a
  // pending rescan never reports it under the previous document's opt-in.
  is_qr_code_detection_enabled_ = false;
  rescan_timer_.Stop();
  follow_up_rescan_timer_.Stop();
  last_reported_signals_.reset();
}

void FacilitatedPaymentsAgent::DidFinishSameDocumentNavigation() {
  OnTriggeredRescan();
}

void FacilitatedPaymentsAgent::DidObserveSoftNavigation(
    blink::SoftNavigationMetricsForReporting metrics) {
  OnTriggeredRescan();
}

void FacilitatedPaymentsAgent::OnDestruct() {
  delete this;
}

void FacilitatedPaymentsAgent::OnTriggeredRescan() {
  if (!is_qr_code_detection_enabled_) {
    return;
  }

  // Reset timer so that DOM scanning waits until scroll or layout activity
  // has settled.
  rescan_timer_.Start(
      FROM_HERE, kRescanDebounceDelay,
      base::BindOnce(&FacilitatedPaymentsAgent::RescanAndReportSignals,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsAgent::RescanAndReportSignals() {
  if (!is_qr_code_detection_enabled_) {
    return;
  }
  ReportSignals(ScanSignals());
}

mojom::HeuristicSignalsPtr FacilitatedPaymentsAgent::ScanSignals() {
  // The scan runs synchronously on the main thread, so its duration is the
  // jank that the heuristics add to the page.
  base::ScopedUmaHistogramTimer timer(
      kHeuristicsLatencyHistogram,
      base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMicrosecondTimes);
  return CalculateHeuristicSignals();
}

void FacilitatedPaymentsAgent::ReportSignals(
    mojom::HeuristicSignalsPtr signals) {
  last_reported_signals_ = signals.Clone();
  const auto& driver = GetDriver();
  if (driver) {
    driver->ReportHeuristicSignals(std::move(signals));
  }
}

void FacilitatedPaymentsAgent::SetDriverForTesting(
    mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> driver) {
  driver_ = std::move(driver);
}

mojom::HeuristicSignalsPtr
FacilitatedPaymentsAgent::CalculateHeuristicSignals() {
  auto signals = mojom::HeuristicSignals::New();
  if (!render_frame()) {
    return signals;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    return signals;
  }
  const blink::WebDocument document = frame->GetDocument();
  if (document.IsNull()) {
    return signals;
  }

  // Every signal is collected, even when the page has a facilitated payment
  // link, so that the browser's metrics see the full signal combination.
  const std::vector<std::string> keywords = GetKeywords();
  signals->has_facilitated_payment_link = HasFacilitatedPaymentLink(document);
  signals->has_square_candidate = HasQrCodeImageCandidate(document);
  signals->url_keyword_match = UrlMatchesKeyword(document, keywords);
  signals->text_keyword_match = TextMatchesKeyword(document, keywords);
  return signals;
}

void FacilitatedPaymentsAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsAgent>
        pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver>&
FacilitatedPaymentsAgent::GetDriver() {
  if (!driver_ && render_frame()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&driver_);
  }
  return driver_;
}

}  // namespace payments::facilitated
