// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    ukm::SourceId ukm_source_id)
    : driver_(*driver),
      optimization_guide_decider_(optimization_guide_decider),
      ukm_source_id_(ukm_source_id) {
  DCHECK(optimization_guide_decider_);
  // TODO(b/314826708): Check if at least 1 GPay linked PIX account is
  // available for the user. If not, do not register the PIX allowlist.
  RegisterPixAllowlist();
}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() = default;

void FacilitatedPaymentsManager::
    DelayedCheckAllowlistAndTriggerPixCodeDetection(const GURL& url,
                                                    int attempt_number) {
  // TODO(b/300332597): If a page navigation takes place, it might be too late,
  // and PIX code detection might have already run on the previous page. Find an
  // earlier point in the page loading sequence of events where the timer could
  // be stopped.
  // Stop the timer in case it is running from a previous page load.
  pix_code_detection_triggering_timer_.Stop();
  switch (GetAllowlistCheckResult(url)) {
    case optimization_guide::OptimizationGuideDecision::kTrue: {
      // The PIX code detection should be triggered after `kPageLoadWaitTime`.
      // Time spent waiting for the allowlist checking infra should be accounted
      // for.
      base::TimeDelta trigger_pix_code_detection_delay =
          std::max(base::Seconds(0),
                   kPageLoadWaitTime - (attempt_number - 1) *
                                           kOptimizationGuideDeciderWaitTime);
      pix_code_detection_triggering_timer_.Start(
          FROM_HERE, trigger_pix_code_detection_delay,
          base::BindOnce(&FacilitatedPaymentsManager::TriggerPixCodeDetection,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kUnknown: {
      if (attempt_number >= kMaxAttemptsForAllowlistCheck) {
        break;
      }
      pix_code_detection_triggering_timer_.Start(
          FROM_HERE, kOptimizationGuideDeciderWaitTime,
          base::BindOnce(&FacilitatedPaymentsManager::
                             DelayedCheckAllowlistAndTriggerPixCodeDetection,
                         weak_ptr_factory_.GetWeakPtr(), url,
                         attempt_number + 1));
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kFalse:
      break;
  }
}

void FacilitatedPaymentsManager::RegisterPixAllowlist() const {
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST});
}

optimization_guide::OptimizationGuideDecision
FacilitatedPaymentsManager::GetAllowlistCheckResult(const GURL& url) const {
  // Since the optimization guide decider integration corresponding to PIX
  // merchant lists are allowlists for the question "Can this site be
  // optimized?", a match on the allowlist answers the question with "yes".
  // Therefore, `kTrue` indicates that `url` is allowed for running PIX code
  // detection. If the optimization type was not registered in time when we
  // queried it, it will be `kUnknown`.
  return optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST,
      /*optimization_metadata=*/nullptr);
}

void FacilitatedPaymentsManager::TriggerPixCodeDetection() {
  StartPixCodeDetectionLatencyTimer();
  driver_->TriggerPixCodeDetection(
      base::BindOnce(&FacilitatedPaymentsManager::ProcessPixCodeDetectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::ProcessPixCodeDetectionResult(
    mojom::PixCodeDetectionResult result) const {
  ukm::builders::FacilitatedPayments_PixCodeDetectionResult(ukm_source_id_)
      .SetResult(static_cast<uint8_t>(result))
      .SetLatencyInMillis(GetPixCodeDetectionLatencyInMillis())
      .Record(ukm::UkmRecorder::Get());
}

void FacilitatedPaymentsManager::StartPixCodeDetectionLatencyTimer() {
  pix_code_detection_latency_measuring_timestamp_ = base::TimeTicks::Now();
}

int64_t FacilitatedPaymentsManager::GetPixCodeDetectionLatencyInMillis() const {
  return (base::TimeTicks::Now() -
          pix_code_detection_latency_measuring_timestamp_)
      .InMilliseconds();
}

}  // namespace payments::facilitated
