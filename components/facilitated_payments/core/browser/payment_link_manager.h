// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/strike_databases/payment_link_suggestion_strike_database.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
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

// The different types of FOP that can be selected by the user.
enum FopType {
  // An external payment app that is not integrated with GPay.
  kExternalPaymentApp = 0,
  // A GPay payment instrument, such as an eWallet.
  kGPayInstrument = 1,
};

// Contains information about the FOP selected by the user.
// Based on the fop_type, different fields in this struct will be populated.
// If fop_type is kGPayInstrument, instrument_id will be set.
// If fop_type is kExternalPaymentApp, package_name and activity_name will be
// set.
struct SelectedFopData {
  explicit SelectedFopData(int64_t instrument_id)
      : fop_type(kGPayInstrument), instrument_id(instrument_id) {}

  SelectedFopData(const std::string& package_name,
                  const std::string& activity_name)
      : fop_type(kExternalPaymentApp),
        instrument_id(0),
        package_name(package_name),
        activity_name(activity_name) {}

  const FopType fop_type;
  const int64_t instrument_id;
  const std::string package_name;
  const std::string activity_name;
};

class FacilitatedPaymentsClient;
class FacilitatedPaymentsInitiatePaymentResponseDetails;
enum class AvailableEwalletsConfiguration;

// A cross-platform interface that manages the push payment flow triggered by
// payment links. It is owned by `FacilitatedPaymentsDriver`.
class PaymentLinkManager {
 public:
  PaymentLinkManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  PaymentLinkManager(const PaymentLinkManager&) = delete;
  PaymentLinkManager& operator=(const PaymentLinkManager&) = delete;
  virtual ~PaymentLinkManager();

  // Initiates the payment link push payment flow for a given payment link in a
  // certain page. The `payment_link_url` contains all the information to
  // initialize a payment. And the `page_url` is the url of a page where the
  // payment link is detected. More details on payment links can be found at
  // https://github.com/aneeshali/paymentlink/blob/main/docs/explainer.md.
  virtual void TriggerPaymentLinkPushPayment(const GURL& payment_link_url,
                                             const GURL& page_url,
                                             ukm::SourceId ukm_source_id);

  // Resets `this` to initial state.
  void Reset();

 private:
  friend class PaymentLinkManagerTestApi;

  // Performs various specific pre-checks for the eWallet flow.
  bool CanTriggerEwalletPaymentFlow(const GURL& page_url);

  // Determines and populates the list of supported eWallets for a payment link.
  void RetrieveSupportedEwallets(const GURL& payment_link_url);

  // Performs various specific pre-checks for the A2A flow.
  bool CanTriggerAppPaymentFlow(const GURL& page_url);

  // Lazily initializes an API client and returns a pointer to it. Returns a
  // pointer to the existing API client, if one is already initialized. The
  // PaymentLinkManager owns this API client. This method can return
  // `nullptr` if the API client fails to initialize, e.g., if the
  // `RenderFrameHost` has been destroyed.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Called when user selects any FOP to pay with.
  void OnFopSelected(SelectedFopData selected_fop_data);

  // Called when user selects the eWallet account to pay with.
  void OnEwalletAccountSelected(int64_t selected_instrument_id);

  // Called when user selects a payment app to pay with.
  void OnPaymentAppSelected(std::string_view package_name,
                            std::string_view activity_name);

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
  void OnUiScreenEvent(UiEvent ui_event_type);

  // Updates the `ui_state_` value and triggers dismissal.
  void DismissPrompt();

  // Updates the `ui_state_` value and triggers showing the payment options.
  void ShowPaymentLinkPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      std::unique_ptr<FacilitatedPaymentsAppInfoList> app_suggestions,
      base::OnceCallback<void(payments::facilitated::SelectedFopData)>
          on_fop_selected);

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

  // Returns payment link FOP selector type based on the availability of payment
  // apps and eWallets.
  PaymentLinkFopSelectorTypes GetPaymentLinkFopSelectorType();

  // A list of eWallets that support the payment link provided in
  // TriggerPaymentLinkPushPayment().
  //
  // This vector is populated in RetrieveSupportedEwallets() by filtering the
  // available eWallets based on their support for the given payment link.
  //
  // It will be empty:
  //  * Before RetrieveSupportedEwallets() is called.
  //  * If TriggerPaymentLinkPushPayment() is called with an invalid or
  //  unsupported payment link.
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

  // Stores the time when payment link payment flow is triggered.
  base::TimeTicks payment_flow_triggered_timestamp_;

  // True indicates that the eWallet selected by the user is bound to the
  // device. This field is used for logging purposes.
  bool is_device_bound_for_logging_ = false;

  // True indicates that there is at least one payment app that is available for
  // payment.
  bool is_payment_app_available_ = false;

  // True indicates that there is at least one eWallet that is available for
  // payment.
  bool is_ewallet_available_ = false;

  // A timer to make UI changes.
  base::OneShotTimer ui_timer_;

  // Strike database used to check whether to prompt the FOP selector or not.
  std::unique_ptr<PaymentLinkSuggestionStrikeDatabase> strike_database_;

  base::WeakPtrFactory<PaymentLinkManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_MANAGER_H_
