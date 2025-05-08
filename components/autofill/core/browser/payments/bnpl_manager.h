// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill::payments {

using UpdateSuggestionsCallback =
    base::RepeatingCallback<void(std::vector<Suggestion>,
                                 AutofillSuggestionTriggerSource)>;

struct BnplFetchVcnResponseDetails;
struct BnplFetchUrlResponseDetails;
struct BnplIssuerContext;

// Owned by BrowserAutofillManager. There is one instance of this class per
// frame. This class manages the flow for BNPL to complete a payment
// transaction.
class BnplManager {
 public:
  using OnBnplVcnFetchedCallback = base::OnceCallback<void(const CreditCard&)>;

  explicit BnplManager(BrowserAutofillManager* browser_autofill_manager);
  BnplManager(const BnplManager& other) = delete;
  BnplManager& operator=(const BnplManager& other) = delete;
  virtual ~BnplManager();

  // Retrieve supported BNPL issuers.
  static const std::array<std::string_view, 2>& GetSupportedBnplIssuerIds();

  // Initializes the BNPL flow, which includes UI shown to the user to select an
  // issuer, a possible ToS dialog, and redirecting to the selected issuer's
  // website before filling the form, if the flow succeeds.
  // `final_checkout_amount` is the checkout amount extracted from the page (in
  // micros). `on_bnpl_vcn_fetched_callback` is the callback that should be run
  // if the flow is completed successfully, to fill the form with the VCN that
  // will facilitate the BNPL transaction.
  virtual void OnDidAcceptBnplSuggestion(
      uint64_t final_checkout_amount,
      OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback);

  // Notifies the BNPL manager that suggestion generation has been requested
  // with the given `trigger_source`. This must be called before
  // `OnSuggestionsShown()` and `OnAmountExtractionReturned()`, so that the
  // manager can update suggestions for buy-now-pay-later.
  virtual void NotifyOfSuggestionGeneration(
      const AutofillSuggestionTriggerSource trigger_source);

  // Runs after credit card suggestions are shown and collects the current
  // shown suggestions and a callback for updating the suggestions. This must
  // be called after `NotifyOfSuggestionGeneration()`, so that the manager can
  // update suggestions for buy-now-pay-later.
  virtual void OnSuggestionsShown(
      base::span<const Suggestion> suggestions,
      UpdateSuggestionsCallback update_suggestions_callback);

  // Runs after amount extraction completion and collects the amount extraction
  // result. This must be called after `NotifyOfSuggestionGeneration()`, so
  // that the manager can update suggestions for buy-now-pay-later.
  virtual void OnAmountExtractionReturned(
      const std::optional<uint64_t>& extracted_amount);

  // Determines if autofill BNPL is supported.
  // Returns true if:
  // 1. The BNPL feature flag is enabled.
  // 2. The client has an `AutofillOptimizationGuide` assigned.
  // 3. The URL being visited is within the BNPL issuer allowlist.
  bool IsEligibleForBnpl() const;

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

    // The user's current app locale.
    std::string app_locale;

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

    // The BNPL partner the user is trying to retrieve the VCN from. Set when
    // the user selects an issuer in the issuer selection dialog. If it is an
    // unlinked issuer, and the user links it, `issuer` will still be the
    // unlinked version throughout the flow. The instrument ID returned from the
    // Payments server during the linking will be what is used to retrieve the
    // VCN, and then afterwards the linked version will be synced down to Chrome
    // for future flows.
    BnplIssuer issuer;

    // The final checkout amount on the page (in micros), used for the ongoing
    // BNPL flow.
    uint64_t final_checkout_amount;

    // The callback that will fill the fetched BNPL VCN into the form.
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback;
  };

  // This function makes the appropriate call to the payments server to fetch
  // the VCN details for the BNPL issuer selected in the BNPL manager. `url` is
  // the last URL navigated to inside of the pop-up, and will contain
  // information that the issuer needs to fetch the virtual card details for the
  // flow.
  void FetchVcnDetails(GURL url);

  // The callback after the FetchVcnDetails call returns from the server. The
  // callback contains the result of the call as well as the VCN details.
  void OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult result,
                           const BnplFetchVcnResponseDetails& response_details);

  // Cancels in-progress requests to `PaymentsNetworkInterface` and resets the
  // BNPL flow state. Also invalidates `BnplManager` weak pointers from the
  // factory.
  void Reset();

  // Runs after users select a BNPL issuer, and will redirect to plan selection
  // or terms of services depending on the issuer.
  void OnIssuerSelected(BnplIssuer selected_issuer);

  // This function makes the appropriate call to the payments server to get info
  // from the server for creating an instrument for the selected issuer.
  void GetDetailsForCreateBnplPaymentInstrument();

  // The callback after
  // `PaymentsNetworkInterface::GetDetailsForCreateBnplPaymentInstrument` calls.
  // The callback contains the result of the call as well as `context_token`
  // for providing information from this request that is needed by
  // `CreateBnplPaymentInstrumentRequest` and `legal_message` to be displayed
  // to users.
  void OnDidGetDetailsForCreateBnplPaymentInstrument(
      PaymentsAutofillClient::PaymentsRpcResult result,
      std::string context_token,
      LegalMessageLines legal_message);

  // Runs when a linked issuer is selected by the user. Will load risk data
  // if it is not cached, and then call the functions for fetching issuer
  // redirect urls.
  void LoadRiskDataForFetchingRedirectUrl();

  // Runs after the risk data is loaded. Will set the risk data for the flow,
  // and redirect to 'FetchRedirectUrl()' for sending the fetch redirect url
  // request.
  void OnRiskDataLoadedAfterIssuerSelectionDialogAcceptance(
      const std::string& risk_data);

  // Makes the appropriate call to the payments server to fetch the redirect
  // urls from the selected issuer.
  void FetchRedirectUrl();

  // The callback after
  // `PaymentsNetworkInterface::GetBnplPaymentInstrumentForFetchingUrl()` calls.
  // The callback contains the result of the call as well as `context_token`
  // and urls from the issuer for redirecting and result checking.
  void OnRedirectUrlFetched(PaymentsAutofillClient::PaymentsRpcResult result,
                            const BnplFetchUrlResponseDetails& response);

  // The callback after `PaymentsWindowManager::InitBnplFlow()` calls.
  // The callback contains the result of the flow and will continue to
  // VCN fetching if successful. `url` is the last URL that was navigated to
  // inside of the pop-up.
  void OnPopupWindowCompleted(PaymentsWindowManager::BnplFlowResult result,
                              GURL url);

  // Combines `responses` from suggestion shown event and amount extraction,
  // and try to show card suggestions with buy-now-pay-later suggestion.
  void MaybeUpdateSuggestionsWithBnpl(
      const AutofillSuggestionTriggerSource trigger_source,
      std::vector<std::variant<SuggestionsShownResponse,
                               std::optional<uint64_t>>> responses);

  // Callback triggered when the user accepts the ToS dialog. It will first load
  // risk data, and once risk data is loaded, initiate a call to the Payments
  // servers to create a BNPL instrument for the selected issuer. Risk data is
  // loaded here because the CreateBnplPaymentInstrument request is the first
  // time it is needed during the BNPL flow.
  void OnTosDialogAccepted();

  // Callback triggered once the prefetched risk data from the flow
  // initialization has finished loading.
  void OnPrefetchedRiskDataLoaded(const std::string& risk_data);

  // Callback triggered once risk data has finished loading after ToS dialog
  // acceptance, to set the risk data and trigger
  // `CreateBnplPaymentInstrument()`.
  void OnRiskDataLoadedAfterTosDialogAcceptance(const std::string& risk_data);

  // Sends a request to the Payments servers to create a BNPL payment
  // instrument.
  void CreateBnplPaymentInstrument();

  // Callback after attempting to create a BNPL payment instrument. `result`
  // indicates success/failure; `instrument_id` is the new ID if successful. If
  // successful, stores the ID and fetches the redirect URL.
  void OnBnplPaymentInstrumentCreated(
      PaymentsAutofillClient::PaymentsRpcResult result,
      std::string instrument_id);

  // Return all BNPL Issuer contexts including eligibility in order of:
  // eligible + linked, eligible + unlinked, uneligible + linked,
  // uneligible + unlinked.
  std::vector<BnplIssuerContext> GetSortedBnplIssuerContext();

  const PaymentsAutofillClient& payments_autofill_client() const {
    return const_cast<BnplManager*>(this)->payments_autofill_client();
  }

  PaymentsAutofillClient& payments_autofill_client() {
    return *browser_autofill_manager_->client().GetPaymentsAutofillClient();
  }

  // The associated browser autofill manager.
  const raw_ref<BrowserAutofillManager> browser_autofill_manager_;

  // The state for the ongoing flow. Only present if there is a flow currently
  // ongoing. Set when a flow is initiated, and reset upon flow completion.
  std::unique_ptr<OngoingFlowState> ongoing_flow_state_;

  // Set to true after the first time a BNPL suggestion not being shown is
  // logged. Ensures that logging occurs only once per page load.
  bool has_logged_bnpl_suggestion_not_shown_reason_ = false;

  // Callback to collect the current shown suggestion list and checkout
  // amount, and insert BNPL suggestion if the amount is eligible.
  std::optional<base::RepeatingCallback<void(
      std::variant<SuggestionsShownResponse, std::optional<uint64_t>>)>>
      update_suggestions_barrier_callback_;

  base::WeakPtrFactory<BnplManager> weak_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_MANAGER_H_
