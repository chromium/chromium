// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

struct BnplFetchVcnResponseDetails;

// Owned by PaymentsAutofillClient. There is one instance of this class per Web
// Contents. This class manages the flow for BNPL to complete a payment
// transaction.
class BnplManager {
 public:
  using OnBnplVcnFetchedCallback = base::OnceCallback<void(const CreditCard&)>;

  explicit BnplManager(PaymentsAutofillClient* payments_autofill_client);
  BnplManager(const BnplManager& other) = delete;
  BnplManager& operator=(const BnplManager& other) = delete;
  ~BnplManager();

  // Initializes the BNPL flow, which includes UI shown to the user to select an
  // issuer, a possible ToS dialog, and redirecting to the selected issuer's
  // website before filling the form, if the flow succeeds.
  // `final_checkout_amount` is the checkout amount extracted from the page (in
  // micros). `on_bnpl_vcn_fetched_callback` is the callback that should be run
  // if the flow is completed successfully, to fill the form with the VCN that
  // will facilitate the BNPL transaction.
  void InitBnplFlow(uint64_t final_checkout_amount,
                    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback);

  // This function attempts to convert a string representation of a monetary
  // value in dollars into a uint64_t by parsing it as a double and multiplying
  // the result by 1,000,000. It assumes the input uses a decimal point ('.') as
  // the separator for fractional values (not a decimal comma). The function
  // only supports English-style monetary representations like $, USD, etc.
  // Multiplication by 1,000,000 is done to represent the monetary value in
  // micro-units (1 dollar = 1,000,000 micro-units), which is commonly used in
  // systems that require high precision for financial calculations.
  std::optional<uint64_t> MaybeParseAmountToMonetaryMicroUnits(
      const std::string& amount);

 private:
  friend class BnplManagerTestApi;

  // A collection of information that represents the state of an ongoing BNPL
  // flow.
  struct OngoingFlowState {
    OngoingFlowState();
    OngoingFlowState(const OngoingFlowState&) = delete;
    OngoingFlowState& operator=(const OngoingFlowState&) = delete;
    ~OngoingFlowState();

    // Billing customer number for the user's Google Payments account.
    int64_t billing_customer_number;

    // Risk data contains the fingerprint data for the user and the device.
    std::string risk_data;

    // BNPL Issuer Data - Populated when user selects a BNPL issuer
    // Instrument ID used by the server to identify a specific BNPL issuer. This
    // is selected by the user.
    std::string instrument_id;

    // Context token shared between client and Payments server.
    std::string context_token;

    // URL that the the partner redirected the user to after finishing the BNPL
    // flow on the partner website.
    GURL redirect_url;

    // The final checkout amount on the page (in micros), used for the ongoing
    // BNPL flow.
    uint64_t final_checkout_amount;

    // The callback that will fill the fetched BNPL VCN into the form.
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback;
  };

  // This function makes the appropriate call to the payments server to fetch
  // the VCN details for the BNPL issuer selected in the BNPL manager.
  void FetchVcnDetails();

  // The callback after the FetchVcnDetails call returns from the server. The
  // callback contains the result of the call as well as the VCN details.
  void OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult result,
                           const BnplFetchVcnResponseDetails& response_details);

  // The associated payments autofill client.
  const raw_ref<PaymentsAutofillClient> payments_autofill_client_;

  // The state for the ongoing flow. Only present if there is a flow currently
  // ongoing. Set when a flow is initiated, and reset upon flow completion.
  std::unique_ptr<OngoingFlowState> ongoing_flow_state_;

  base::WeakPtrFactory<BnplManager> weak_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_
