// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_

#include <cstring>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace payments::facilitated {

class FacilitatedPaymentsApiClient;
class FacilitatedPaymentsClient;
class FacilitatedPaymentsDriver;

// A cross-platform interface that manages the flow of payments for non-form
// based form-of-payments between the browser and the Payments platform. It is
// owned by `FacilitatedPaymentsDriver`.
class FacilitatedPaymentsManager {
 public:
  FacilitatedPaymentsManager(
      FacilitatedPaymentsDriver* driver,
      FacilitatedPaymentsClient* client,
      std::unique_ptr<FacilitatedPaymentsApiClient> api_client,
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
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           NoPixPaymentPromptWhenApiClientNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ShowsPixPaymentPromptWhenApiClientAvailable);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      ApiClientNotAvailable_RiskDataNotLoaded_DoesNotTriggerLoadRiskData);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      ApiClientAvailable_RiskDataNotLoaded_TriggersLoadRiskData);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PaymentNotOfferedReason_RiskDataEmpty);

  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      ApiClientAvailable_RiskDataLoaded_DoesNotTriggerLoadRiskData);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DoesNotRetrieveClientTokenIfPixPaymentPromptRejected);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RetrievesClientTokenIfPixPaymentPromptAccepted);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           GetClientTokenHistogram_ClientTokenNotEmpty);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           GetClientTokenHistogram_ClientTokenEmpty);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TriggerPixDetectionOnDomContentLoadedExpDisabled_Ukm);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TriggerPixDetectionOnDomContentLoadedExpEnabled_Ukm);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ResettingPreventsPayment);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerWithPixPaymentsDisabledTest,
      ValidPixCodeDetectionResult_HasPixAccounts_ApiClientNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
      ValidPixCodeDetectionResult_HasPixAccounts_ApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
      ValidPixCodeDetectionResult_InvalidPixCodeString_ApiClientNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
      InvalidPixCodeDetectionResultDoesNotTriggerApiClient);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           AbsenceOfPixAccountsDoesNotTriggerApiClient);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           UnavailabilityOfPdmDoesNotTriggerApiClient);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           ValidPixDetectionResultToPixPaymentPromptShown);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           PixCodeValidated_ApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           PaymentNotOfferedReason_CodeValidatorReturnsFalse);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           PixCodeValidationFailed_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           PaymentNotOfferedReason_CodeValidatorFailed);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           ApiAvailabilityHistogram);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
      PixCodeValidatorTerminatedUnexpectedly_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           PaymentNotOfferedReason_ApiNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
                           SendInitiatePaymentRequest);

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
  void ProcessPixCodeDetectionResult(mojom::PixCodeDetectionResult result,
                                     const std::string& pix_code);

  // Called by the utility process after validation of the `pix_code`. If the
  // utility processes has disconnected (e.g., due to a crash in the validation
  // code), then `is_pix_code_valid` contains an error string instead of the
  // boolean validation result.
  void OnPixCodeValidated(std::string pix_code,
                          base::expected<bool, std::string> is_pix_code_valid);

  // Starts `pix_code_detection_latency_measuring_timestamp_`.
  void StartPixCodeDetectionLatencyTimer();

  int64_t GetPixCodeDetectionLatencyInMillis() const;

  // Called after checking whether the facilitated payment API is available. If
  // the API is not available, the user should not be prompted to make a
  // payment.
  void OnApiAvailabilityReceived(bool is_api_available);

  // Invoked when risk data is fetched.
  void OnRiskDataLoaded(const std::string& risk_data);

  // Called after showing the PIX the payment prompt.
  void OnPixPaymentPromptResult(bool is_prompt_accepted,
                                int64_t selected_instrument_id);

  // Called after retrieving the client token from the facilitated payment API.
  // If not empty, the client token can be used for initiating payment.
  void OnGetClientToken(std::vector<uint8_t> client_token);

  // Makes a payment request to the Payments server after the user has selected
  // the account for making the payment.
  void SendInitiatePaymentRequest();

  // Called after receiving the `result` of the initiate payment call. The
  // `response_details` contains the action token used for payment.
  void OnInitiatePaymentResponseReceived(
      autofill::AutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details);

  // Calling `Reset` has no effect in tests. Adding this method to specifically
  // test `Resets` in tests.
  void ResetForTesting();

  // Owner.
  raw_ref<FacilitatedPaymentsDriver> driver_;

  // Indirect owner.
  raw_ref<FacilitatedPaymentsClient> client_;

  // The client for the facilitated payment API.
  std::unique_ptr<FacilitatedPaymentsApiClient> api_client_;

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

  // Measures the time taken to check the availability of the facilitated
  // payments API client.
  base::TimeTicks api_availability_check_latency_;

  // Measures the time take to load the client token from the facilitated
  // payments API client.
  base::TimeTicks get_client_token_loading_latency_;

  // Contains the details required for the `InitiatePayment` request to be sent
  // to the Payments server. Its ownership is transferred to
  // `FacilitatedPaymentsInitiatePaymentRequest` in
  // `SendInitiatePaymentRequest`. `Reset` destroys the existing instance, and
  // creates a new instance.
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      initiate_payment_request_details_;

  // Informs whether this instance was created in a test.
  bool is_test_ = false;

  // Utility process validator for PIX code strings.
  data_decoder::DataDecoder utility_process_validator_;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
