// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace payments::facilitated {

ContentFacilitatedPaymentsDriver::ContentFacilitatedPaymentsDriver(
    FacilitatedPaymentsClient* client,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<SecurityChecker> security_checker)
    : FacilitatedPaymentsDriver(std::make_unique<FacilitatedPaymentsManager>(
                                    /*driver=*/this,
                                    client,
                                    GetFacilitatedPaymentsApiClientCreator(
                                        render_frame_host->GetGlobalId()),
                                    optimization_guide_decider),
                                std::make_unique<EwalletManager>()),
      render_frame_host_id_(render_frame_host->GetGlobalId()),
      security_checker_(std::move(security_checker)) {}

ContentFacilitatedPaymentsDriver::~ContentFacilitatedPaymentsDriver() = default;

void ContentFacilitatedPaymentsDriver::TriggerPixCodeDetection(
    base::OnceCallback<void(mojom::PixCodeDetectionResult, const std::string&)>
        callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  if (render_frame_host && render_frame_host->IsActive()) {
    GetAgent(render_frame_host)->TriggerPixCodeDetection(std::move(callback));
  }
}

// TODO(crbug.com/40280186): Add test for this method once FPManager refactoring
// is done.
void ContentFacilitatedPaymentsDriver::HandlePaymentLink(const GURL& url) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (!security_checker_->IsSecureForPaymentLinkHandling(*render_frame_host)) {
    return;
  }

  TriggerEwalletPushPayment(
      /*payment_link_url=*/url,
      /*page_url=*/render_frame_host->GetLastCommittedURL());
}

void ContentFacilitatedPaymentsDriver::SetPaymentLinkHandlerReceiver(
    mojo::PendingReceiver<mojom::PaymentLinkHandler> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>&
ContentFacilitatedPaymentsDriver::GetAgent(
    content::RenderFrameHost* render_frame_host) {
  if (!agent_.is_bound()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent_);
  }
  return agent_;
}

}  // namespace payments::facilitated
