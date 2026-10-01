// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace payments::facilitated {

FacilitatedPaymentsAgent::FacilitatedPaymentsAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
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

  mojom::HeuristicSignalsPtr signals = CalculateHeuristicSignals();
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
  // All signals are false until the DOM checks are added.
  return mojom::HeuristicSignals::New();
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
