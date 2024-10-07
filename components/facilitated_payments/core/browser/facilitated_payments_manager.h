// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_

#include <cstring>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace payments::facilitated {

class FacilitatedPaymentsClient;
class FacilitatedPaymentsDriver;

// A cross-platform interface that manages the flow of payments for non-form
// based form-of-payments between the browser and the Payments platform. It is
// owned by `FacilitatedPaymentsDriver`.
// TODO(crbug.com/369898977): Rename `FacilitatedPaymentsManager` to be PIX
// specific and update the class level comment.
class FacilitatedPaymentsManager {
 public:
  FacilitatedPaymentsManager(
      FacilitatedPaymentsDriver* driver,
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  FacilitatedPaymentsManager(const FacilitatedPaymentsManager&) = delete;
  FacilitatedPaymentsManager& operator=(const FacilitatedPaymentsManager&) =
      delete;
  virtual ~FacilitatedPaymentsManager();

  // Resets `this` to initial state. Cancels any alive async callbacks.
  void Reset();

  // Checks whether the `render_frame_host_url` is allowlisted and validates the
  // `pix_code` before trigger the Pix payments flow. Note: If the Pix payment
  // flow has already been triggered by the other code detection methods like
  // DOM search then this method is a no-op.
  void OnPixCodeCopiedToClipboard(const GURL& render_frame_host_url,
                                  const std::string& pix_code,
                                  ukm::SourceId ukm_source_id);

 private:
  friend class FacilitatedPaymentsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RegisterPixAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DOMSearch_CheckAllowlistResultUnknown_PixCodeDetectionNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DOMSearch_CheckAllowlistResultShortDelay_UrlInAllowlist_PixCodeDetectionTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DOMSearch_CheckAllowlistResultShortDelay_UrlNotInAllowlist_PixCodeDetectionNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DOMSearch_CheckAllowlistResultLongDelay_UrlInAllowlist_PixCodeDetectionNotTriggered);
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
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ShowsPixPaymentPrompt_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      PixPaymentPromptNotAccepted_LoadRiskDataNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RiskDataNotEmpty_HistogramsLogged);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RiskDataEmpty_HistogramsLogged);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PixPaymentPromptAccepted_TriggersLoadRiskData);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PaymentNotOfferedReason_RiskDataEmpty);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           RiskDataNotEmpty_GetClientTokenCalled);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           GetClientTokenHistogram_ClientTokenNotEmpty);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           GetClientTokenHistogram_ClientTokenEmpty);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PixPaymentPromptAccepted_ProgressSceenShown);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PixPaymentPromptRejected_ProgressSceenNotShown);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           OnGetClientToken_ClientTokenEmpty_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TriggerPixDetectionOnDomContentLoadedExpDisabled_Ukm);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TriggerPixDetectionOnDomContentLoadedExpEnabled_Ukm);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ResettingPreventsPayment);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           CopyTrigger_UrlInAllowlist_PixValidationTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CopyTrigger_UrlNotInAllowlist_PixValidationNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      CopyTriggerHappenedBeforeDOMSearch_ApiClientIsAvailableCalledOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      DOMSearchHappenedBeforeCopyTrigger_ApiClientIsAvailableCalledOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      ValidPixCodeDetectionResult_HasPixAccounts_ApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      ValidPixCodeDetectionResult_InvalidPixCodeString_ApiClientNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      InvalidPixCodeDetectionResultDoesNotTriggerApiClient);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PixPrefTurnedOff_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           NoPixAccounts_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           NoPaymentsDataManager_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ValidPixDetectionResultToPixPaymentPromptShown);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ApiClientTriggeredAfterPixCodeValidation);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PaymentNotOfferedReason_CodeValidatorReturnsFalse);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PixCodeValidationFailed_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PaymentNotOfferedReason_CodeValidatorFailed);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ApiAvailabilityHistogram);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      PixCodeValidatorTerminatedUnexpectedly_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           PaymentNotOfferedReason_ApiNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           SendInitiatePaymentRequest);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      OnInitiatePaymentResponseReceived_FailureResponse_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           OnPurchaseActionPositiveResult_UiPromptDismissed);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           OnPurchaseActionNegativeResult_UiPromptDismissed);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           InvokePurchaseActionCompleted_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           OnInitiatePaymentResponseReceived_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TransactionSuccess_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TransactionAbandonedAfterInvokePurchaseAction_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TransactionFailedAfterInvokePurchaseAction_HistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      FOPSelectorNotShown_TransactionResultHistogramNotLogged);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           ApiClientInitializedLazily);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           HandlesFailureToLazilyInitializeApiClient);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTestInLandscapeMode,
                           PixPayflowBlockedWhenFlagDisabled);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTestInLandscapeMode,
                           HistogramForPaymentNotOfferedReason);

  // Register optimization guide deciders for PIX. It is an allowlist of URLs
  // where we attempt PIX code detection.
  void RegisterPixAllowlist() const;

  // Queries the allowlist for the `url`. The result could be:
  // 1. In the allowlist
  // 2. Not in the allowlist
  // 3. Infra for querying is not ready
  // Returns true if the result is [1].
  bool IsMerchantAllowlisted(const GURL& url) const;

  // Called by the utility process after validation of the `pix_code`. If the
  // utility processes has disconnected (e.g., due to a crash in the validation
  // code), then `is_pix_code_valid` contains an error string instead of the
  // boolean validation result. The call to validate the PIX code was made at
  // `start_time`.
  void OnPixCodeValidated(std::string pix_code,
                          base::TimeTicks start_time,
                          base::expected<bool, std::string> is_pix_code_valid);

  // Lazily initializes an API client and returns a pointer to it. Returns a
  // pointer to the existing API client, if one is already initialized. The
  // FacilitatedPaymentManager owns this API client. This method can return
  // `nullptr` if the API client fails to initialize, e.g., if the
  // `RenderFrameHost` has been destroyed.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Called after checking whether the facilitated payment API is available. If
  // the API is not available, the user should not be prompted to make a
  // payment.
  void OnApiAvailabilityReceived(bool is_api_available);

  // Called after showing the PIX the payment prompt.
  void OnPixPaymentPromptResult(bool is_prompt_accepted,
                                int64_t selected_instrument_id);

  // Invoked when risk data is fetched. The call to fetch the risk data was made
  // at `start_time`.
  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data);

  // Called after retrieving the client token from the facilitated payment API.
  // If not empty, the client token can be used for initiating payment.
  void OnGetClientToken(std::vector<uint8_t> client_token);

  // Makes a payment request to the Payments server after the user has selected
  // the account for making the payment.
  void SendInitiatePaymentRequest();

  // Called after receiving the `result` of the initiate payment call. The
  // `response_details` contains the action token used for payment.
  void OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details);

  // Called after receiving the `result` of invoking the purchase manager for
  // payment.
  void OnPurchaseActionResult(
      FacilitatedPaymentsApiClient::PurchaseActionResult result);

  // Owner.
  const raw_ref<FacilitatedPaymentsDriver> driver_;

  // Indirect owner.
  const raw_ref<FacilitatedPaymentsClient> client_;

  // The creator of the facilitated payment API client.
  FacilitatedPaymentsApiClientCreator api_client_creator_;

  // The client for the facilitated payment API.
  std::unique_ptr<FacilitatedPaymentsApiClient> api_client_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  ukm::SourceId ukm_source_id_;

  // Measures the time taken to check the availability of the facilitated
  // payments API client.
  base::TimeTicks api_availability_check_start_time_;

  // Measures the time take to load the client token from the facilitated
  // payments API client.
  base::TimeTicks get_client_token_loading_start_time_;

  // Measures the time take to complete the call to the InitiatePayment backend
  // api.
  base::TimeTicks initiate_payment_network_start_time_;

  // Measures the time take to complete the purchase action.
  base::TimeTicks purchase_action_start_time_;

  // Stores the time when the FOP selector was shown to the user. This is used
  // to calculate the entire transaction latency.
  base::TimeTicks fop_selector_shown_time_;

  // Contains the details required for the `InitiatePayment` request to be sent
  // to the Payments server. Its ownership is transferred to
  // `FacilitatedPaymentsInitiatePaymentRequest` in
  // `SendInitiatePaymentRequest`. `Reset` destroys the existing instance, and
  // creates a new instance.
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      initiate_payment_request_details_;

  // Flag to help determine whether the payflow has been triggered since the
  // last page load. It protects against multiple triggers to initiate the
  // payflow like Pix code detection, copy button click, and copy button
  // double-click.
  bool has_payflow_started_ = false;

  // Utility process validator for PIX code strings.
  data_decoder::DataDecoder utility_process_validator_;

  // The source of the trigger for the facilitated payments form of payment(FOP)
  // selector to show up. It is used for logging purposes. It is set whenever a
  // trigger occurs and reset if the FOP selector is not shown for some reason.
  TriggerSource trigger_source_ = TriggerSource::kUnknown;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
