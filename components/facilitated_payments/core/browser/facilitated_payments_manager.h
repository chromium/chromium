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

class FacilitatedPaymentsDriver;
class FacilitatedPaymentsClient;

// A cross-platform interface that manages the flow of payments for non-form
// based form-of-payments between the browser and the Payments platform. It is
// owned by `FacilitatedPaymentsDriver`.
class FacilitatedPaymentsManager {
 public:
  FacilitatedPaymentsManager(
      FacilitatedPaymentsDriver* driver,
      FacilitatedPaymentsClient* client,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  FacilitatedPaymentsManager(const FacilitatedPaymentsManager&) = delete;
  FacilitatedPaymentsManager& operator=(const FacilitatedPaymentsManager&) =
      delete;
  virtual ~FacilitatedPaymentsManager();

  // Resets `this` to initial state. Cancels any alive async callbacks.
  void Reset();

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
  void DelayedCheckAllowlistAndTriggerPixCodeDetection(
      const GURL& url,
      ukm::SourceId ukm_source_id,
      int attempt_number = 1);

 private:
  // Defined here so they can be accessed by the tests.
  static constexpr base::TimeDelta kOptimizationGuideDeciderWaitTime =
      base::Seconds(0.5);
  static constexpr int kMaxAttemptsForAllowlistCheck = 6;
  static constexpr base::TimeDelta kPageLoadWaitTime = base::Seconds(2);
  static constexpr base::TimeDelta kRetriggerPixCodeDetectionWaitTime =
      base::Seconds(1);
  static constexpr int kMaxAttemptsForPixCodeDetection = 6;

  friend class FacilitatedPaymentsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RegisterPixAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CheckAllowlistResultUnknown_PixCodeDetectionNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CheckAllowlistResultShortDelay_UrlInAllowlist_PixCodeDetectionTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CheckAllowlistResultShortDelay_UrlNotInAllowlist_PixCodeDetectionNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CheckAllowlistResultLongDelay_UrlInAllowlist_PixCodeDetectionNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           NoPixCode_PixCodeNotFoundLoggedAfterMaxAttempts);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           NoPixCode_PixCodeNotFoundLoggedAfterMaxAttempts);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest, NoPixCode_NoUkm);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTestWhenPixCodeExists,
      LongPageLoadDelay_PixCodeNotFoundLoggedAfterMaxAttempts);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTestWhenPixCodeExists,
      LongPageLoadDelay_PixCodeNotFoundLoggedAfterMaxAttempts);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTestWhenPixCodeExists,
                           Ukm);

  // Register optimization guide deciders for PIX. It is an allowlist of URLs
  // where we attempt PIX code detection.
  void RegisterPixAllowlist() const;

  // Queries the allowlist for the `url`. The result could be:
  // 1. In the allowlist
  // 2. Not in the allowlist
  // 3. Infra for querying is not ready
  optimization_guide::OptimizationGuideDecision GetAllowlistCheckResult(
      const GURL& url) const;

  // Calls `TriggerPixCodeDetection` after `delay`.
  void DelayedTriggerPixCodeDetection(base::TimeDelta delay);

  // Asks the renderer to scan the document for a PIX code. The call is made via
  // the `driver_`.
  void TriggerPixCodeDetection();

  // Callback to be called after attempting PIX code detection. `result`
  // represents the result of the document scan.
  void ProcessPixCodeDetectionResult(mojom::PixCodeDetectionResult result);

  // Starts `pix_code_detection_latency_measuring_timestamp_`.
  void StartPixCodeDetectionLatencyTimer();

  int64_t GetPixCodeDetectionLatencyInMillis() const;

  // Owner.
  raw_ref<FacilitatedPaymentsDriver> driver_;

  // Indirect owner.
  raw_ref<FacilitatedPaymentsClient> client_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  ukm::SourceId ukm_source_id_;

  // Counter for the number of attempts at PIX code detection.
  int pix_code_detection_attempt_count_ = 0;

  // Scheduler. Used for check allowlist retries, PIX code detection retries,
  // page load wait, etc.
  base::OneShotTimer pix_code_detection_triggering_timer_;

  // Measures the time taken to scan the document for the PIX code.
  base::TimeTicks pix_code_detection_latency_measuring_timestamp_;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
