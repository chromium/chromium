// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_H_

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
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;

namespace payments::facilitated {

class FacilitatedPaymentsClient;

// A cross-platform interface that manages the Pix payment flow. It is
// owned by `FacilitatedPaymentsDriver`.
class PixManager {
 public:
  PixManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  PixManager(const PixManager&) = delete;
  PixManager& operator=(const PixManager&) = delete;
  virtual ~PixManager();

  // Resets `this` to initial state. Cancels any alive async callbacks.
  void Reset();

  // Checks whether the `render_frame_host_url` is allowlisted and validates the
  // `pix_code` before trigger the Pix payments flow. Note: If the Pix payment
  // flow has already been triggered by the other code detection methods like
  // DOM search then this method is a no-op.
  virtual void OnPixCodeCopiedToClipboard(
      const GURL& render_frame_host_url,
      const url::Origin& render_frame_host_origin,
      const std::string& pix_code,
      ukm::SourceId ukm_source_id);

 private:
  friend class PixManagerTest;
  friend class PixManagerTestForUiScreens;
  friend class PixManagerPaymentsNetworkInterfaceTest;
  // Keep all entries in alphabetical order!
  FRIEND_TEST_ALL_PREFIXES(PixManagerPaymentsNetworkInterfaceTest,
                           OnInitiatePaymentResponseReceived_FailureResponse);
  FRIEND_TEST_ALL_PREFIXES(PixManagerPaymentsNetworkInterfaceTest,
                           SendInitiatePaymentRequest);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerPaymentsNetworkInterfaceTest,
      OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerPaymentsNetworkInterfaceTest,
      OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerPaymentsNetworkInterfaceTest,
      OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerPaymentsNetworkInterfaceTest,
      OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(PixManagerPaymentsNetworkInterfaceTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ApiClientInitializedLazily);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ApiClientTriggeredAfterPixCodeValidation);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           CopyTrigger_UrlInAllowlist_PixValidationTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      CopyTrigger_UrlNotInAllowlist_PixValidationNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      CopyTrigger_UrlNotInAllowlist_PayflowExitedHistogramLogged);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           DismissPrompt);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      ChromeCustomTabWithGboardAsDefaultIme_PixFlowNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      ChromeCustomTabWithGboardNotAsDefaultIme_PixFlowTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      ErrorScreenNotAutoDismissedAfterInvokingPurchaseAction);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           HandlesFailureToLazilyInitializeApiClient);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           LogApiAvailabilityCheckResultAndLatency);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           LogGetClientTokenResultAndLatency);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           LogInitiatePurchaseActionAttempt);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           LogInitiatePurchaseActionResultAndLatency);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           LogTransactionResultAndLatency);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           NoPaymentsDataManager_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PaymentsAutofillTurnedOff_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_PaymentsAutofillTurnedOff);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           UserOptedOut_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           NoPixAccounts_NoApiClientTriggered);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           NoPixPaymentPromptWhenApiClientNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           OnGetClientToken_ClientTokenEmpty_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           OnPixAccountSelected);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      OnPurchaseActionResult_CouldNotInvoke_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      OnPurchaseActionResult_ResultCanceled_UiScreenDismissed);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           OnPurchaseActionResult_ResultOk_UiScreenDismissed);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_ApiClientNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_ClientTokenNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_CodeValidatorFailed);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_InvalidCode);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_NoLinkedAccount);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      PayflowExitedReason_StaticCode_FeatureDisabled_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      PayflowExitedReason_StaticCode_ApiClientAvailabilityChecked);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      NoLinkedAccount_AccountLinkingFlagDisabled_AccountLinkingFlowNotTriggered);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           NoLinkedAccount_AccountLinkingFlowTriggered);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_RiskDataEmpty);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PayflowExitedReason_UserOptedOut);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           InvalidCode_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           CodeValidatorFailed_PixFlowsAbandoned);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           PixFopSelectorShown_HistogramsLogged);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           UserOptedOut_PixPayflowAbandoned);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      ProgressScreenAutoDismissedAfterInvokingPurchaseAction);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           RegisterPixAllowlist);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ResettingPreventsPayment);
  FRIEND_TEST_ALL_PREFIXES(
      PixManagerTestWithAccountLinkingEnabled,
      RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           RiskDataEmpty_HistogramsLogged);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           RiskDataNotEmpty_GetClientTokenCalled);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           RiskDataNotEmpty_HistogramsLogged);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ShowErrorScreen);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ShowPixPaymentPrompt);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ShowProgressScreen);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestWithAccountLinkingEnabled,
                           ShowsPixPaymentPromptWhenApiClientAvailable);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestForUiScreens,
                           NewScreenCouldNotBeShown);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestForUiScreens, NewScreenShown);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestForUiScreens, ScreenClosedByUser);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestForUiScreens, ScreenClosedNotByUser);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestInLandscapeMode,
                           PayflowExitedReason_LandscapeScreenOrientation);
  FRIEND_TEST_ALL_PREFIXES(PixManagerTestInLandscapeMode,
                           PixPayflowBlockedWhenFlagDisabled);

  // Queries the allowlist for the `url`. The result could be:
  // 1. In the allowlist
  // 2. Not in the allowlist
  // 3. Infra for querying is not ready
  // Returns true if the result is [1].
  bool IsMerchantAllowlisted(const GURL& url) const;

  // Called by the utility process after validation of the `pix_code`. If the
  // utility processes has disconnected (e.g., due to a crash in the validation
  // code), then `pix_qr_code_type` contains an error string instead of the
  // PixQrCodeType result. The call to validate the Pix code was made at
  // `start_time`.
  void OnPixCodeValidated(
      std::string pix_code,
      base::TimeTicks start_time,
      base::expected<mojom::PixQrCodeType, std::string> pix_qr_code_type);

  // Lazily initializes an API client and returns a pointer to it. Returns a
  // pointer to the existing API client, if one is already initialized. The
  // PixManager owns this API client. This method can return
  // `nullptr` if the API client fails to initialize, e.g., if the
  // `RenderFrameHost` has been destroyed.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Called after checking whether the facilitated payment API is available. If
  // the API is not available, the user should not be prompted to make a
  // payment. The call to check API availability was made at `start_time`.
  void OnApiAvailabilityReceived(base::TimeTicks start_time,
                                 bool is_api_available);

  // Called when user selects the Pix bank account to pay with. The FOP selector
  // was shown at `fop_selector_shown_timestamp`.
  void OnPixAccountSelected(base::TimeTicks fop_selector_shown_timestamp,
                            int64_t selected_instrument_id);

  // Invoked when risk data is fetched. The call to fetch the risk data was made
  // at `start_time`.
  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data);

  // Called after retrieving the client token from the facilitated payment API.
  // If not empty, the client token can be used for initiating payment. The call
  // to fetch client token was made at `start_time`.
  void OnGetClientToken(base::TimeTicks start_time,
                        std::vector<uint8_t> client_token);

  // Makes a payment request to the Payments server after the user has selected
  // the account for making the payment.
  void SendInitiatePaymentRequest();

  // Called after receiving the `result` of the initiate payment call. The
  // `response_details` contains the action token used for payment. The initiate
  // payment request was sent at `start_time`.
  void OnInitiatePaymentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details);

  // Called after receiving the `result` of invoking the purchase manager for
  // payment. The purchase action was triggered at `start_time`.
  void OnPurchaseActionResult(base::TimeTicks start_time,
                              PurchaseActionResult result);

  // Called by the view to communicate UI events.
  void OnUiScreenEvent(UiEvent ui_event_type);

  // Sets the internal state and triggers dismissal.
  void DismissPrompt();

  // Sets the internal state and triggers showing the Pix payment prompt.
  void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(int64_t)> on_pix_account_selected);

  // Sets the internal state and triggers showing the progress screen.
  void ShowProgressScreen();

  // Sets the internal state and triggers showing the error screen.
  void ShowErrorScreen();

  // Dismisses the FacilitatedPayments bottom sheet if the progress screen is
  // being shown.
  void DismissProgressScreen();

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

  // Stores the time when a user copies a Pix code.
  base::TimeTicks pix_code_copied_timestamp_;

  // A timer to make UI changes.
  base::OneShotTimer ui_timer_;

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

  // Utility process validator for Pix code strings.
  data_decoder::DataDecoder utility_process_validator_;

  // Represents the current state of the UI or the UI state that is intended. In
  // the latter case, the UI state is always updated to reflect the current
  // state via a callback.
  UiState ui_state_ = UiState::kHidden;

  // The origin of the Pix payment page that triggered the payment flow.
  url::Origin pix_payment_page_origin_;

  base::WeakPtrFactory<PixManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_MANAGER_H_
