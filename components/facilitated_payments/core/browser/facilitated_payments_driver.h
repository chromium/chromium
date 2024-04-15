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

class FacilitatedPaymentsManager;

// A cross-platform interface which is a gateway for all PIX payments related
// communication from the browser code to the DOM (`FacilitatedPaymentsAgent`).
// There can be one instance for each outermost main frame. It is only created
// if the main frame is active at the time of load.
class FacilitatedPaymentsDriver {
 public:
  explicit FacilitatedPaymentsDriver(
      std::unique_ptr<FacilitatedPaymentsManager> manager);
  FacilitatedPaymentsDriver(const FacilitatedPaymentsDriver&) = delete;
  FacilitatedPaymentsDriver& operator=(const FacilitatedPaymentsDriver&) =
      delete;
  virtual ~FacilitatedPaymentsDriver();

  // Informs `FacilitatedPaymentsManager` about a navigation that has committed.
  // It is invoked only for the primary main frame by the platform-specific
  // implementation.
  void DidFinishNavigation() const;

  // Informs `FacilitatedPaymentsManager` that the content has finished loading
  // in the primary main frame. It is invoked by the platform-specific
  // implementation.
  virtual void OnContentLoadedInThePrimaryMainFrame(
      const GURL& url,
      ukm::SourceId ukm_source_id) const;

  // Trigger PIX code detection on the page. The `callback` is called after
  // running PIX code detection and is passed a boolean informing whether or not
  // a PIX code was found.
  virtual void TriggerPixCodeDetection(
      base::OnceCallback<void(mojom::PixCodeDetectionResult)> callback) = 0;

 private:
  std::unique_ptr<FacilitatedPaymentsManager> manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_DRIVER_H_
