// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace payments::facilitated {

// Implementation of `FacilitatedPaymentsDriver` for Android/Desktop. It
// is owned by `ContentFacilitatedPaymentsFactory`.
// Each `ContentFacilitatedPaymentsDriver` is associated with exactly one
// `RenderFrameHost` and communicates with exactly one
// `FacilitatedPaymentsAgent` throughout its entire lifetime.
class ContentFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  explicit ContentFacilitatedPaymentsDriver(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ContentFacilitatedPaymentsDriver(const ContentFacilitatedPaymentsDriver&) =
      delete;
  ContentFacilitatedPaymentsDriver& operator=(
      const ContentFacilitatedPaymentsDriver&) = delete;
  ~ContentFacilitatedPaymentsDriver() override;

  // FacilitatedPaymentsDriver:
  void TriggerPixCodeDetection(
      base::OnceCallback<void(bool)> callback) const override;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
