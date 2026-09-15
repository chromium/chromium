// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace payments::facilitated {

ContentFacilitatedPaymentsDriver::ContentFacilitatedPaymentsDriver(
    FacilitatedPaymentsClient* client,
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<SecurityChecker> security_checker,
    ContentFacilitatedPaymentsDriverFactory* factory)
    : payments::facilitated::FacilitatedPaymentsDriver(
          client,
          GetFacilitatedPaymentsApiClientCreator(
              render_frame_host->GetGlobalId())),
      render_frame_host_id_(render_frame_host->GetGlobalId()),
      factory_(factory),
      security_checker_(std::move(security_checker)) {}

ContentFacilitatedPaymentsDriver::~ContentFacilitatedPaymentsDriver() = default;

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

  TriggerPaymentLinkPushPayment(
      /*payment_link_url=*/url,
      /*page_url=*/render_frame_host->GetLastCommittedURL(),
      /*ukm_source_id=*/render_frame_host->GetPageUkmSourceId());
}

bool ContentFacilitatedPaymentsDriver::IsSecureForPaymentHandling() const {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  return security_checker_->IsSecureForPaymentLinkHandling(*render_frame_host);
}

void ContentFacilitatedPaymentsDriver::ReportHeuristicScore(
    double heuristic_score) {
  if (factory_) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_frame_host_id_);
    if (render_frame_host) {
      factory_->OnHeuristicScoreReported(render_frame_host, heuristic_score);
    }
  }
}

void ContentFacilitatedPaymentsDriver::SetPaymentLinkHandlerReceiver(
    mojo::PendingReceiver<mojom::PaymentLinkHandler> pending_receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(pending_receiver));
}

void ContentFacilitatedPaymentsDriver::SetFacilitatedPaymentsDriverReceiver(
    mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsDriver>
        pending_receiver) {
  driver_receiver_.reset();
  driver_receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>&
ContentFacilitatedPaymentsDriver::GetFacilitatedPaymentsAgent() {
  if (!agent_.is_bound()) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_frame_host_id_);
    if (render_frame_host) {
      render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent_);
    }
  }
  return agent_;
}

void ContentFacilitatedPaymentsDriver::SetFacilitatedPaymentsAgentForTesting(
    mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent> agent) {
  agent_ = std::move(agent);
}

void ContentFacilitatedPaymentsDriver::SetFactoryForTesting(
    ContentFacilitatedPaymentsDriverFactory* factory) {
  factory_ = factory;
}

}  // namespace payments::facilitated
