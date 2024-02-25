// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/functional/callback.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace payments::facilitated {

class FaciliatedPaymentsManager;

ContentFacilitatedPaymentsDriver::ContentFacilitatedPaymentsDriver(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    content::RenderFrameHost* render_frame_host)
    : FacilitatedPaymentsDriver(std::make_unique<FacilitatedPaymentsManager>(
          this,
          optimization_guide_decider,
          render_frame_host->GetPageUkmSourceId())),
      render_frame_host_(*render_frame_host) {}

ContentFacilitatedPaymentsDriver::~ContentFacilitatedPaymentsDriver() = default;

void ContentFacilitatedPaymentsDriver::TriggerPixCodeDetection(
    base::OnceCallback<void(mojom::PixCodeDetectionResult)> callback) {
  GetAgent()->TriggerPixCodeDetection(std::move(callback));
}

const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>&
ContentFacilitatedPaymentsDriver::GetAgent() {
  if (!agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_);
  }
  return agent_;
}

}  // namespace payments::facilitated
