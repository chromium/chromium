// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"

namespace blink {
class AssociatedInterfaceRegistry;
struct SoftNavigationMetricsForReporting;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace payments::facilitated {

// Autonomous agent in the Renderer process that listens to Blink lifecycle
// events (layout shifts, scrolling, page load) and calculates heuristic scores
// indicating the probability of a payment QR code being present on the page.
class FacilitatedPaymentsAgent : public content::RenderFrameObserver,
                                 public mojom::FacilitatedPaymentsAgent {
 public:
  static constexpr base::TimeDelta kRescanDebounceDelay = base::Seconds(1);

  FacilitatedPaymentsAgent(content::RenderFrame* render_frame,
                           blink::AssociatedInterfaceRegistry* registry);

  FacilitatedPaymentsAgent(const FacilitatedPaymentsAgent&) = delete;
  FacilitatedPaymentsAgent& operator=(const FacilitatedPaymentsAgent&) = delete;

  ~FacilitatedPaymentsAgent() override;

  // mojom::FacilitatedPaymentsAgent:
  void SetQrCodeDetectionEnabled(bool enabled) override;

  // content::RenderFrameObserver:
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidChangeScrollOffset(blink::mojom::ScrollType) override;
  void DidFinishLoad() override;
  void DidFinishSameDocumentNavigation() override;
  void DidObserveSoftNavigation(
      blink::SoftNavigationMetricsForReporting metrics) override;
  void OnDestruct() override;

  // Resets the debounce timer to schedule a rescan after page activity settles.
  void OnTriggeredRescan();

  // Evaluates DOM heuristics and reports the cumulative probability score to
  // the browser process.
  void RescanAndReportScore();

  // For testing:
  void SetDriverForTesting(
      mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> driver);
  base::OneShotTimer& GetTimerForTesting() { return rescan_timer_; }
  bool is_qr_code_detection_enabled() const {
    return is_qr_code_detection_enabled_;
  }

 protected:
  // Virtual for testing to mock DOM score calculation without bringing up
  // Blink.
  virtual double CalculateHeuristicScore();

 private:
  // Binds the Mojo receiver to handle requests from the browser process.
  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsAgent>
          pending_receiver);

  // Lazily obtains or returns the remote driver interface to the browser.
  const mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver>& GetDriver();

  // Debounce timer to prevent running expensive heuristics while the user
  // is actively scrolling or layout is shifting rapidly.
  base::OneShotTimer rescan_timer_;

  // Tracks whether autonomous QR code detection heuristics are enabled.
  bool is_qr_code_detection_enabled_ = true;

  // Mojo receiver for `mojom::FacilitatedPaymentsAgent`.
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsAgent> receiver_{this};

  // Mojo remote to the browser-side `mojom::FacilitatedPaymentsDriver`.
  mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> driver_;

  base::WeakPtrFactory<FacilitatedPaymentsAgent> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_
