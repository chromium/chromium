// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace payments::facilitated {

class EwalletManager;
class FacilitatedPaymentsManager;

// A cross-platform interface which is a gateway for all PIX payments related
// communication from the browser code to the DOM (`FacilitatedPaymentsAgent`).
// Also, it receives notifications when payment links are detected from renderer
// process during DOM construction . There can be one instance for each
// outermost main frame. It is only created if the main frame is active at the
// time of load.
//
// TODO(crbug.com/371059457): `FacilitatedPaymentsDriver` is currently an
// abstract class. Considering migrating it to a pure interface and use delegate
// to handle common logics shared by cross-platform.
class FacilitatedPaymentsDriver {
 public:
  FacilitatedPaymentsDriver(std::unique_ptr<FacilitatedPaymentsManager> manager,
                            std::unique_ptr<EwalletManager> ewallet_manager);
  FacilitatedPaymentsDriver(const FacilitatedPaymentsDriver&) = delete;
  FacilitatedPaymentsDriver& operator=(const FacilitatedPaymentsDriver&) =
      delete;
  virtual ~FacilitatedPaymentsDriver();

  // Informs `FacilitatedPaymentsManager` that a navigation related event has
  // taken place. The navigation could be to the currently displayed page, or
  // away from the currently displayed page. It is invoked only for the primary
  // main frame by the platform-specific implementation.
  void DidNavigateToOrAwayFromPage() const;

  // Trigger PIX code detection on the page. The `callback` is called after
  // running PIX code detection.
  virtual void TriggerPixCodeDetection(
      base::OnceCallback<void(mojom::PixCodeDetectionResult,
                              const std::string&)> callback) = 0;

  // Inform the `FacilitatedPaymentsManager` about `copied_text` being copied to
  // the clipboard. It is invoked only for the primary main frame.
  virtual void OnTextCopiedToClipboard(const GURL& render_frame_host_url,
                                       const std::u16string& copied_text,
                                       ukm::SourceId ukm_source_id);

  // Inform the `EwalletManager` to trigger the eWallet push payment flow. The
  // payment information is included in the `payment_link_url` contained by the
  // page with URL as `page_url`.
  virtual void TriggerEwalletPushPayment(const GURL& payment_link_url,
                                         const GURL& page_url);

  virtual void SetEwalletManagerForTesting(
      std::unique_ptr<EwalletManager> ewallet_manager);

 private:
  std::unique_ptr<FacilitatedPaymentsManager> manager_;

  std::unique_ptr<EwalletManager> ewallet_manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_
