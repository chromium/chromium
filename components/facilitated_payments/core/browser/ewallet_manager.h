// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/strike_databases/payment_link_suggestion_strike_database.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
}

namespace autofill {
class Ewallet;
}

namespace payments::facilitated {

class FacilitatedPaymentsClient;
class FacilitatedPaymentsInitiatePaymentResponseDetails;
enum class AvailableEwalletsConfiguration;

// A cross-platform interface that manages the eWallet push payment flow. It is
// owned by `FacilitatedPaymentsDriver`.
class EwalletManager {
 public:
  EwalletManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  EwalletManager(const EwalletManager&) = delete;
  EwalletManager& operator=(const EwalletManager&) = delete;
  virtual ~EwalletManager();

  // Initiates the eWallet push payment flow for a given payment link in a
  // certain page. The `payment_link_url` contains all the information to
  // initialize a payment. And the `page_url` is the url of a page where the
  // payment link is detected. More details on payment links can be found at
  // https://github.com/aneeshali/paymentlink/blob/main/docs/explainer.md.
  virtual void TriggerEwalletPushPayment(const GURL& payment_link_url,
                                         const GURL& page_url,
                                         ukm::SourceId ukm_source_id);

  // Resets `this` to initial state.
  void Reset();

 private:
  friend class EwalletManagerTestApi;

  // Lazily initializes an API client and returns a pointer to it. Returns a
  // pointer to the existing API client, if one is already initialized. The
  // EwalletManager owns this API client. This method can return
  // `nullptr` if the API client fails to initialize, e.g., if the
  // `RenderFrameHost` has been destroyed.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Called after checking whether the facilitated payment API is available. If
  // the API is not available, the user should not be prompted to make a
  // payment.  The call to check the availability of API was made at
  // `start_time`.
  void OnApiAvailabilityReceived(base::TimeTicks start_time,
                                 bool is_api_available);

  // Called when user selects the eWallet account to pay with.
  void OnEwalletAccountSelected(int64_t selected_instrument_id);

  // Invoked when risk data is fetched. The call to fetch the risk data was
  // made at `start_time`. `risk_data` is the fetched risk data.
  void OnRiskDataLoaded(base::TimeTicks start_time,
                        const std::string& risk_data);

  // Called after retrieving the client token from the facilitated payment API.
  // If not empty, the client token can be used for initiating payment.
  // The call was made at `start_time`.
  void OnGetClientToken(base::TimeTicks start_time,
                        std::vector<uint8_t> client_token);

  // Makes a payment request to the Payments server after the user has selected
  // the eWallet for making the payment.
  void SendInitiatePaymentRequest();

  // Called after receiving the `result` of the initiate payment call. The
  // `response_details` contains the action token used for payment.
  // The call was made at `start_time`.
  void OnInitiatePaymentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details);

  // Called after receiving the `result` of invoking the purchase manager for
  // payment. The call to invoke purchase manager was made at `start_time`.
  void OnTransactionResult(base::TimeTicks start_time,
                           PurchaseActionResult result);

  // Called by the view to communicate UI events.
  void OnUiEvent(UiEvent ui_event_type);

  // Updates the `ui_state_` value and triggers dismissal.
  void DismissPrompt();

  // Updates the `ui_state_` value and triggers showing the eWallet payment
  // prompt.
  void ShowEwalletPaymentPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      base::OnceCallback<void(int64_t)> on_ewallet_account_selected);

  // Updates the `ui_state_` value and triggers showing the progress screen.
  void ShowProgressScreen();

  // Updates the `ui_state_` value and triggers showing the error screen.
  void ShowErrorScreen();

  // Returns the `AvailableEwalletsConfiguration` for this user profile.
  AvailableEwalletsConfiguration GetAvailableEwalletsConfiguration();

  // Dismisses the FacilitatedPayments bottom sheet if the progress screen is
  // being shown.
  void DismissProgressScreen();

  // Retrieves the strike database for payment link suggestion. This can return
  // nullptr so check before using.
  PaymentLinkSuggestionStrikeDatabase* GetOrCreateStrikeDatabase();

  // A list of eWallets that support the payment link provided in
  // TriggerEwalletPushPayment().
  //
  // This vector is populated in TriggerEwalletPushPayment() by filtering the
  // available eWallets based on their support for the given payment link.
  //
  // It will be empty:
  //  * Before TriggerEwalletPushPayment() is called.
  //  * If TriggerEwalletPushPayment() is called with an invalid or unsupported
  //    payment link.
  //  * After a call to Reset().
  std::vector<autofill::Ewallet> supported_ewallets_;

  // Indirect owner. `FacilitatedPaymentsClient` owns
  // `FacilitatedPaymentsDriver` which owns `this`.
  const raw_ref<FacilitatedPaymentsClient> client_;

  // The creator of the facilitated payment API client.
  FacilitatedPaymentsApiClientCreator api_client_creator_;

  // The client for the facilitated payment API.
  std::unique_ptr<FacilitatedPaymentsApiClient> api_client_;

  // The optimization guide decider to help determine whether the current URL
  // is eligible for eWallet push payments.
  const raw_ref<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // The scheme of the detected payment link.
  PaymentLinkValidator::Scheme scheme_;

  // Contains the details required for the `InitiatePayment` request to be sent
  // to the Payments server. Its ownership is transferred to
  // `FacilitatedPaymentsInitiatePaymentRequest` in
  // `SendInitiatePaymentRequest`. `Reset` destroys the existing instance, and
  // creates a new instance.
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      initiate_payment_request_details_;

  // Represents the current state of the UI or the UI state that is intended. In
  // the latter case, the UI state is always updated to reflect the current
  // state via a callback.
  UiState ui_state_ = UiState::kHidden;

  // Stores the time when eWallet payment flow is triggered.
  base::TimeTicks payment_flow_triggered_timestamp_;

  // True indicates that the eWallet selected by the user is bound to the
  // device. This field is used for logging purposes.
  bool is_device_bound_for_logging_ = false;

  // A timer to make UI changes.
  base::OneShotTimer ui_timer_;

  // Strike database used to check whether to prompt the FOP selector or not.
  std::unique_ptr<PaymentLinkSuggestionStrikeDatabase> strike_database_;

  base::WeakPtrFactory<EwalletManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_
