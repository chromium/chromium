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
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"

class GURL;

namespace autofill {
class Ewallet;
}

namespace payments::facilitated {

class FacilitatedPaymentsClient;
class FacilitatedPaymentsInitiatePaymentResponseDetails;

// A cross-platform interface that manages the eWallet push payment flow. It is
// owned by `FacilitatedPaymentsDriver`.
class EwalletManager {
 public:
  EwalletManager(FacilitatedPaymentsClient* client,
                 FacilitatedPaymentsApiClientCreator api_client_creator);
  EwalletManager(const EwalletManager&) = delete;
  EwalletManager& operator=(const EwalletManager&) = delete;
  virtual ~EwalletManager();

  // Initiates the eWallet push payment flow for a given payment link in a
  // certain page. The `payment_link_url` contains all the information to
  // initialize a payment. And the `page_url` is the url of a page where the
  // payment link is detected. More details on payment links can be found at
  // https://github.com/aneeshali/paymentlink/blob/main/docs/explainer.md.
  virtual void TriggerEwalletPushPayment(const GURL& payment_link_url,
                                         const GURL& page_url);

  // Resets `this` to initial state.
  void Reset();

 private:
  friend class EwalletManagerTestApi;

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

  // Called after the user interacts with the eWallet payment prompt.
  // `is_prompt_accepted` indicates whether the user selects an eWallet FOP or
  // dismisses the prompt.
  void OnEwalletPaymentPromptResult(bool is_prompt_accepted,
                                    int64_t selected_instrument_id);

  // Invoked after risk data is fetched. `risk_data` is the fetched risk data.
  void OnRiskDataLoaded(const std::string& risk_data);

  // Called after retrieving the client token from the facilitated payment API.
  // If not empty, the client token can be used for initiating payment.
  void OnGetClientToken(std::vector<uint8_t> client_token);

  // Makes a payment request to the Payments server after the user has selected
  // the eWallet for making the payment.
  void SendInitiatePaymentRequest();

  // Called after receiving the `result` of the initiate payment call. The
  // `response_details` contains the action token used for payment.
  void OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details);

  // Called after receiving the `result` of invoking the purchase manager for
  // payment.
  void OnTransactionResult(
      FacilitatedPaymentsApiClient::PurchaseActionResult result);

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

  // Contains the details required for the `InitiatePayment` request to be sent
  // to the Payments server. Its ownership is transferred to
  // `FacilitatedPaymentsInitiatePaymentRequest` in
  // `SendInitiatePaymentRequest`. `Reset` destroys the existing instance, and
  // creates a new instance.
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      initiate_payment_request_details_;

  base::WeakPtrFactory<EwalletManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_
