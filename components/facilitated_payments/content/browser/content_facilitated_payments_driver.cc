// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace payments::facilitated {

ContentFacilitatedPaymentsDriver::ContentFacilitatedPaymentsDriver(
    FacilitatedPaymentsClient* client,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    content::RenderFrameHost* render_frame_host)
    : FacilitatedPaymentsDriver(std::make_unique<FacilitatedPaymentsManager>(
          /*driver=*/this,
          client,
          CreateFacilitatedPaymentsApiClient(render_frame_host),
          optimization_guide_decider)),
      render_frame_host_(*render_frame_host) {}

ContentFacilitatedPaymentsDriver::~ContentFacilitatedPaymentsDriver() = default;

void ContentFacilitatedPaymentsDriver::TriggerPixCodeDetection(
    base::OnceCallback<void(mojom::PixCodeDetectionResult, const std::string&)>
        callback) {
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
