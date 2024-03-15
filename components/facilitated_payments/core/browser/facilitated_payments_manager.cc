// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver,
    FacilitatedPaymentsClient* client,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : driver_(*driver),
      client_(*client),
      optimization_guide_decider_(optimization_guide_decider) {
  DCHECK(optimization_guide_decider_);
  // TODO(b/314826708): Check if at least 1 GPay linked PIX account is
  // available for the user. If not, do not register the PIX allowlist.
  RegisterPixAllowlist();
}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() = default;

void FacilitatedPaymentsManager::Reset() {
  pix_code_detection_attempt_count_ = 0;
  ukm_source_id_ = 0;
  weak_ptr_factory_.InvalidateWeakPtrs();
  pix_code_detection_triggering_timer_.Stop();
}

void FacilitatedPaymentsManager::
    DelayedCheckAllowlistAndTriggerPixCodeDetection(const GURL& url,
                                                    ukm::SourceId ukm_source_id,
                                                    int attempt_number) {
  Reset();
  switch (GetAllowlistCheckResult(url)) {
    case optimization_guide::OptimizationGuideDecision::kTrue: {
      ukm_source_id_ = ukm_source_id;
      // The PIX code detection should be triggered after `kPageLoadWaitTime`.
      // Time spent waiting for the allowlist checking infra should be accounted
      // for.
      base::TimeDelta trigger_pix_code_detection_delay =
          std::max(base::Seconds(0),
                   kPageLoadWaitTime - (attempt_number - 1) *
                                           kOptimizationGuideDeciderWaitTime);
      DelayedTriggerPixCodeDetection(trigger_pix_code_detection_delay);
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
                         weak_ptr_factory_.GetWeakPtr(), url, ukm_source_id,
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

void FacilitatedPaymentsManager::DelayedTriggerPixCodeDetection(
    base::TimeDelta delay) {
  pix_code_detection_triggering_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&FacilitatedPaymentsManager::TriggerPixCodeDetection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::TriggerPixCodeDetection() {
  pix_code_detection_attempt_count_++;
  StartPixCodeDetectionLatencyTimer();
  driver_->TriggerPixCodeDetection(
      base::BindOnce(&FacilitatedPaymentsManager::ProcessPixCodeDetectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FacilitatedPaymentsManager::ProcessPixCodeDetectionResult(
    mojom::PixCodeDetectionResult result) {
  // If a PIX code was not found, re-trigger PIX code detection after a short
  // duration to allow async content to load completely.
  if (result == mojom::PixCodeDetectionResult::kPixCodeNotFound &&
      pix_code_detection_attempt_count_ < kMaxAttemptsForPixCodeDetection) {
    DelayedTriggerPixCodeDetection(kRetriggerPixCodeDetectionWaitTime);
    return;
  }
  ukm::builders::FacilitatedPayments_PixCodeDetectionResult(ukm_source_id_)
      .SetResult(static_cast<uint8_t>(result))
      .SetLatencyInMillis(GetPixCodeDetectionLatencyInMillis())
      .SetAttempts(pix_code_detection_attempt_count_)
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
