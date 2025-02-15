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
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

class BnplIssuer;

namespace payments {

using UpdateSuggestionsCallback =
    base::RepeatingCallback<void(std::vector<Suggestion>,
                                 AutofillSuggestionTriggerSource)>;

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

  // Retrieve supported BNPL issuers.
  static const std::array<std::string_view, 2>& GetSupportedBnplIssuerIds();

  // Initializes the BNPL flow, which includes UI shown to the user to select an
  // issuer, a possible ToS dialog, and redirecting to the selected issuer's
  // website before filling the form, if the flow succeeds.
  // `final_checkout_amount` is the checkout amount extracted from the page (in
  // micros). `on_bnpl_vcn_fetched_callback` is the callback that should be run
  // if the flow is completed successfully, to fill the form with the VCN that
  // will facilitate the BNPL transaction.
  void InitBnplFlow(uint64_t final_checkout_amount,
                    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback);

  // Notifies the BNPL manager that suggestion generation has been requested
  // with the given `trigger_source`. This must be called before
  // `OnSuggestionsShown()` and `OnAmountExtractionReturned()`, so that the
  // manager can update suggestions for buy-now-pay-later.
  void NotifyOfSuggestionGeneration(
      const AutofillSuggestionTriggerSource trigger_source);

  // Runs after credit card suggestions are shown and collects the current
  // shown suggestions and a callback for updating the suggestions. This must
  // be called after `NotifyOfSuggestionGeneration()`, so that the manager can
  // update suggestions for buy-now-pay-later.
  void OnSuggestionsShown(
      base::span<const Suggestion> suggestions,
      UpdateSuggestionsCallback update_suggestions_callback);

  // Runs after amount extraction completion and collects the amount extraction
  // result. This must be called after `NotifyOfSuggestionGeneration()`, so
  // that the manager can update suggestions for buy-now-pay-later.
  void OnAmountExtractionReturned(
      const std::optional<uint64_t>& extracted_amount);

  // Returns the supported country codes for BNPL.
  static std::set<std::string> GetBnplSupportedCountries();

  // Returns if there is at least one synced BNPL issuer and if the BNPL
  // feature is enabled. Does not check for user's locale.
  bool ShouldShowBnplSettingsToggle() const;

 private:
  friend class BnplManagerTestApi;
  friend class BnplManagerTest;

  using SuggestionsShownResponse =
      std::tuple<std::vector<Suggestion>, UpdateSuggestionsCallback>;

  // A collection of information that represents the state of an ongoing BNPL
  // flow.
  struct OngoingFlowState {
    OngoingFlowState();
    OngoingFlowState(const OngoingFlowState&) = delete;
    OngoingFlowState& operator=(const OngoingFlowState&) = delete;
    ~OngoingFlowState();

    // Billing customer number for the user's Google Payments account.
    int64_t billing_customer_number;

    // BNPL Issuer Data - Populated when user selects a BNPL issuer
    // Instrument ID used by the server to identify a specific BNPL issuer. This
    // is selected by the user.
    std::string instrument_id;

    // Risk data contains the fingerprint data for the user and the device.
    std::string risk_data;

    // Context token shared between client and Payments server.
    std::string context_token;

    // URL that the the partner redirected the user to after finishing the BNPL
    // flow on the partner website.
    GURL redirect_url;

    // The ID of the BNPL partner the user is trying to retrieve the VCN from.
    std::string issuer_id;

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

  // Combines `responses` from suggestion shown event and amount extraction,
  // and try to show card suggestions with buy-now-pay-later suggestion.
  void MaybeUpdateSuggestionsWithBnpl(
      const AutofillSuggestionTriggerSource trigger_source,
      std::vector<std::variant<SuggestionsShownResponse,
                               std::optional<uint64_t>>> responses);

  // The associated payments autofill client.
  const raw_ref<PaymentsAutofillClient> payments_autofill_client_;

  // The state for the ongoing flow. Only present if there is a flow currently
  // ongoing. Set when a flow is initiated, and reset upon flow completion.
  std::unique_ptr<OngoingFlowState> ongoing_flow_state_;

  // Callback to collect the current shown suggestion list and checkout
  // amount, and insert BNPL suggestion if the amount is eligible.
  std::optional<base::RepeatingCallback<void(
      std::variant<SuggestionsShownResponse, std::optional<uint64_t>>)>>
      update_suggestions_barrier_callback_;

  base::WeakPtrFactory<BnplManager> weak_factory_{this};
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_
