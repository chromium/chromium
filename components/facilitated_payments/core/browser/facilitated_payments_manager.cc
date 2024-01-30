// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : driver_(*driver),
      optimization_guide_decider_(optimization_guide_decider) {
  DCHECK(optimization_guide_decider_);
  // TODO(b/314826708): Check if at least 1 GPay linked PIX account is
  // available for the user. If not, do not register the PIX allowlist.
  RegisterPixOptimizationGuide();
}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() = default;

void FacilitatedPaymentsManager::DidFinishLoad(const GURL& url) const {
  if (!ShouldDetectPixCode(url)) {
    return;
  }
  driver_->TriggerPixCodeDetection(
      base::BindOnce(&FacilitatedPaymentsManager::ProcessPixCodeDetectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool FacilitatedPaymentsManager::ShouldDetectPixCode(const GURL& url) const {
  // Since the optimization guide decider integration corresponding to PIX
  // merchant lists are allowlists for the question "Can this site be
  // optimized?", a match on the allowlist answers the question with "yes".
  // Therefore, ...::kTrue indicates that `url` is allowed for running PIX code
  // detection. If the optimization type was not registered in time when we
  // queried it, it will be `kUnknown`, so the default functionality in this
  // case will be to not run PIX code detection on the webpage.
  return optimization_guide_decider_->CanApplyOptimization(
             url, optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST,
             /*optimization_metadata=*/nullptr) ==
         optimization_guide::OptimizationGuideDecision::kTrue;
}

void FacilitatedPaymentsManager::ProcessPixCodeDetectionResult(
    bool pix_code_found) const {}

void FacilitatedPaymentsManager::RegisterPixOptimizationGuide() const {
  // Register the PIX allowlist.
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST});
}

}  // namespace payments::facilitated
