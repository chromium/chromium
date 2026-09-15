// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/facilitated_payments/payment_link_handler.mojom.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content
namespace payments::facilitated {

class ContentFacilitatedPaymentsDriverFactory;
class FacilitatedPaymentsClient;
class SecurityChecker;

// Implementation of `FacilitatedPaymentsDriver` for Android/Desktop. It
// is owned by `ContentFacilitatedPaymentsFactory`.
// Each `ContentFacilitatedPaymentsDriver` is associated with exactly one
// `RenderFrameHost` and communicates with exactly one
// `FacilitatedPaymentsAgent` throughout its entire lifetime.
class ContentFacilitatedPaymentsDriver
    : public FacilitatedPaymentsDriver,
      public mojom::PaymentLinkHandler,
      public mojom::FacilitatedPaymentsDriver {
 public:
  ContentFacilitatedPaymentsDriver(
      FacilitatedPaymentsClient* client,
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<SecurityChecker> security_checker,
      ContentFacilitatedPaymentsDriverFactory* factory = nullptr);
  ContentFacilitatedPaymentsDriver(const ContentFacilitatedPaymentsDriver&) =
      delete;
  ContentFacilitatedPaymentsDriver& operator=(
      const ContentFacilitatedPaymentsDriver&) = delete;
  ~ContentFacilitatedPaymentsDriver() override;

  // mojom::PaymentLinkHandler:
  void HandlePaymentLink(const GURL& url) override;

  // mojom::FacilitatedPaymentsDriver:
  // Receives the autonomous heuristic score from `FacilitatedPaymentsAgent` in
  // the renderer process.
  void ReportHeuristicScore(double heuristic_score) override;

  void SetPaymentLinkHandlerReceiver(
      mojo::PendingReceiver<mojom::PaymentLinkHandler> pending_receiver);

  // Binds the associated receiver for `mojom::FacilitatedPaymentsDriver`.
  void SetFacilitatedPaymentsDriverReceiver(
      mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsDriver>
          pending_receiver);

  // Returns the associated remote for calling `mojom::FacilitatedPaymentsAgent`
  // in the renderer process.
  const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>&
  GetFacilitatedPaymentsAgent();

  // Overrides the remote agent for testing.
  void SetFacilitatedPaymentsAgentForTesting(
      mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent> agent);

  // Sets the factory for testing.
  void SetFactoryForTesting(ContentFacilitatedPaymentsDriverFactory* factory);

 private:
  // FacilitatedPaymentsDriver:
  bool IsSecureForPaymentHandling() const override;

  // The ID of the frame to which this driver is associated.
  const content::GlobalRenderFrameHostId render_frame_host_id_;

  // Factory owning this driver.
  raw_ptr<ContentFacilitatedPaymentsDriverFactory> factory_ = nullptr;

  mojo::Receiver<mojom::PaymentLinkHandler> receiver_{this};
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsDriver> driver_receiver_{
      this};
  mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent> agent_;

  // Helps with checking the security properties of the webpage.
  std::unique_ptr<SecurityChecker> security_checker_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
