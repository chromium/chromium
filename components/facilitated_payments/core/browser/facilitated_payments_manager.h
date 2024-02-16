// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace payments::facilitated {

inline constexpr base::TimeDelta kPageLoadWaitTime = base::Seconds(2);
inline constexpr base::TimeDelta kOptimizationGuideDeciderWaitTime =
    base::Seconds(0.5);
inline constexpr int kMaxAttemptsForAllowlistCheck = 6;

class FacilitatedPaymentsDriver;

// A cross-platform interface that manages the flow of PIX payments between the
// browser and the Payments platform. It is owned by
// `FacilitatedPaymentsDriver`.
class FacilitatedPaymentsManager {
 public:
  FacilitatedPaymentsManager(
      FacilitatedPaymentsDriver* driver,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      ukm::SourceId ukm_source_id);
  FacilitatedPaymentsManager(const FacilitatedPaymentsManager&) = delete;
  FacilitatedPaymentsManager& operator=(const FacilitatedPaymentsManager&) =
      delete;
  virtual ~FacilitatedPaymentsManager();

  // Initiates the PIX payments flow on the browser. There are 2 steps involved:
  // 1. Query the allowlist to check if PIX code detection should be run on the
  // page. It is possible that the infrastructure that supports querying the
  // allowlist is not ready when the page loads. In this case, we query again
  // after `kOptimizationGuideDeciderWaitTime`, and repeat
  // `kMaxAttemptsForAllowlistCheck` times. If the infrastructure is still not
  // ready, we do not run PIX code detection. `attempt_number` is an internal
  // counter for the number of attempts at querying.
  // 2. Trigger PIX code detection on the page after `kPageLoadWaitTime`. The
  // delay allows async content to load on the page. It also prevents PIX code
  // detection negatively impacting page load performance.
  void DelayedCheckAllowlistAndTriggerPixCodeDetection(const GURL& url,
                                                       int attempt_number = 1);

 private:
  friend class FacilitatedPaymentsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TestRegisterPixAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerMetricsTest,
      TestProcessPixCodeDetectionResult_VerifyResultAndLatencyUkmLogged);

  // Register optimization guide deciders for PIX. It is an allowlist of URLs
  // where we attempt PIX code detection.
  void RegisterPixAllowlist() const;

  // Queries the allowlist for the `url`. The result could be:
  // 1. In the allowlist
  // 2. Not in the allowlist
  // 3. Infra for querying is not ready
  optimization_guide::OptimizationGuideDecision GetAllowlistCheckResult(
      const GURL& url) const;

  void TriggerPixCodeDetection();

  // Callback to be called after attempting PIX code detection. `pix_code_found`
  // informs whether or not PIX code was found on the page.
  void ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult result) const;

  void StartPixCodeDetectionLatencyTimer();

  int64_t GetPixCodeDetectionLatencyInMillis() const;

  raw_ref<FacilitatedPaymentsDriver> driver_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  const ukm::SourceId ukm_source_id_;

  base::OneShotTimer pix_code_detection_triggering_timer_;

  // Measures the time taken to scan the document for the PIX code.
  base::TimeTicks pix_code_detection_latency_measuring_timestamp_;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
