// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace payments::facilitated {

class PaymentLinkManager;
class FacilitatedPaymentsClient;
class PixManager;

// A cross-platform interface which is a gateway for all Facilitated Payments
// related communication between the browser and the DOM. There can be one
// instance for each outermost main frame. It is only created if the main frame
// is active at the time of load.
//
// TODO(crbug.com/371059457): `FacilitatedPaymentsDriver` is currently an
// abstract class. Considering migrating it to a pure interface and use delegate
// to handle common logics shared by cross-platform.
class FacilitatedPaymentsDriver {
 public:
  FacilitatedPaymentsDriver(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator);
  FacilitatedPaymentsDriver(const FacilitatedPaymentsDriver&) = delete;
  FacilitatedPaymentsDriver& operator=(const FacilitatedPaymentsDriver&) =
      delete;
  virtual ~FacilitatedPaymentsDriver();

  // Informs `PixManager` that a navigation related event has
  // taken place. The navigation could be to the currently displayed page, or
  // away from the currently displayed page. It is invoked only for the primary
  // main frame by the platform-specific implementation.
  void DidNavigateToOrAwayFromPage() const;

  // Inform the `PixManager` about `copied_text` being copied to
  // the clipboard. It is invoked only for the primary main frame.
  virtual void OnTextCopiedToClipboard(
      const GURL& render_frame_host_url,
      const url::Origin& render_frame_host_origin,
      const std::u16string& copied_text,
      ukm::SourceId ukm_source_id);

  // Inform the `PaymentLinkManager` to trigger the payment link push payment
  // flow. The payment information is included in the `payment_link_url`
  // contained by the page with URL as `page_url`.
  virtual void TriggerPaymentLinkPushPayment(const GURL& payment_link_url,
                                             const GURL& page_url,
                                             ukm::SourceId ukm_source_id);

  virtual void SetPixManagerForTesting(std::unique_ptr<PixManager> pix_manager);
  virtual void SetPaymentLinkManagerForTesting(
      std::unique_ptr<PaymentLinkManager> payment_link_manager);

 private:
  const raw_ref<FacilitatedPaymentsClient> facilitated_payments_client_;
  FacilitatedPaymentsApiClientCreator api_client_creator_;
  std::unique_ptr<PixManager> pix_manager_;
  std::unique_ptr<PaymentLinkManager> payment_link_manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_
