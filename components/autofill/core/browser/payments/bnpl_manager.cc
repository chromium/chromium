// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/data_model/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

namespace {

// Returns true if the `extracted_amount_in_micros` is supported by
// `bnpl_issuer`.
bool ShouldShowBnplOptionForIssuer(const BnplIssuer& bnpl_issuer,
                                   uint64_t extracted_amount_in_micros) {
  // Check Affirm eligibility with currency set to USD.
  // For MVP, BNPL will only targeting to US users and support USD.
  if (bnpl_issuer.issuer_id() == kBnplAffirmIssuerId &&
      bnpl_issuer.IsEligibleAmount(extracted_amount_in_micros, "USD") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableBuyNowPayLaterForAffirm)) {
    return true;
  }

  // Check Zip eligibility with currency set to USD.
  // For MVP, BNPL will only targeting to US users and support USD.
  if (bnpl_issuer.issuer_id() == kBnplZipIssuerId &&
      bnpl_issuer.IsEligibleAmount(extracted_amount_in_micros, "USD") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableBuyNowPayLaterForZip)) {
    return true;
  }

  return false;
}

}  // namespace

BnplManager::OngoingFlowState::OngoingFlowState() = default;

BnplManager::OngoingFlowState::~OngoingFlowState() = default;

BnplManager::BnplManager(PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

BnplManager::~BnplManager() = default;

// static
const std::array<std::string_view, 2>&
BnplManager::GetSupportedBnplIssuerIds() {
  static const std::array<std::string_view, 2> kBnplIssuers = {
      kBnplAffirmIssuerId, kBnplZipIssuerId};
  return kBnplIssuers;
}

void BnplManager::InitBnplFlow(
    uint64_t final_checkout_amount,
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback) {
  ongoing_flow_state_ = std::make_unique<OngoingFlowState>();

  ongoing_flow_state_->final_checkout_amount = final_checkout_amount;
  ongoing_flow_state_->on_bnpl_vcn_fetched_callback =
      std::move(on_bnpl_vcn_fetched_callback);

  // TODO(crbug.com/356443046): Add integration for the BNPL dialogs.
}

void BnplManager::NotifyOfSuggestionGeneration(
    const AutofillSuggestionTriggerSource trigger_source) {
  update_suggestions_barrier_callback_ = base::BarrierCallback<
      std::variant<SuggestionsShownResponse, std::optional<uint64_t>>>(
      2U, base::BindOnce(&BnplManager::MaybeUpdateSuggestionsWithBnpl,
                         weak_factory_.GetWeakPtr(), trigger_source));
}

void BnplManager::OnSuggestionsShown(
    base::span<const Suggestion> suggestions,
    UpdateSuggestionsCallback update_suggestions_callback) {
  if (update_suggestions_barrier_callback_.has_value()) {
    update_suggestions_barrier_callback_->Run(SuggestionsShownResponse(
        std::vector<Suggestion>(std::begin(suggestions), std::end(suggestions)),
        std::move(update_suggestions_callback)));
  }
}

void BnplManager::OnAmountExtractionReturned(
    const std::optional<uint64_t>& extracted_amount) {
  if (update_suggestions_barrier_callback_.has_value()) {
    update_suggestions_barrier_callback_->Run(extracted_amount);
  }
}

void BnplManager::FetchVcnDetails() {
  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.instrument_id = ongoing_flow_state_->instrument_id;
  request_details.risk_data = ongoing_flow_state_->risk_data;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.redirect_url = ongoing_flow_state_->redirect_url;
  request_details.issuer_id = ongoing_flow_state_->issuer_id;

  payments_autofill_client_->GetPaymentsNetworkInterface()
      ->GetBnplPaymentInstrumentForFetchingVcn(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnVcnDetailsFetched,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnVcnDetailsFetched(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const BnplFetchVcnResponseDetails& response_details) {
  // TODO(crbug.com/378518604): Implement OnVcnDetailsFetched() to fill the form
  // from the VCN details that were fetched.

  ongoing_flow_state_.reset();
}

void BnplManager::MaybeUpdateSuggestionsWithBnpl(
    const AutofillSuggestionTriggerSource trigger_source,
    std::vector<std::variant<SuggestionsShownResponse, std::optional<uint64_t>>>
        responses) {
  update_suggestions_barrier_callback_ = std::nullopt;

  SuggestionsShownResponse* suggestions_shown_response = nullptr;
  std::optional<uint64_t>* extracted_amount = nullptr;
  for (auto& response : responses) {
    if (std::holds_alternative<SuggestionsShownResponse>(response)) {
      suggestions_shown_response =
          std::get_if<SuggestionsShownResponse>(&response);
    } else {
      extracted_amount = std::get_if<std::optional<uint64_t>>(&response);
    }
  }

  // TODO(crbug.com/392162610): Add protection so that this function will only
  // be triggered after completion of suggestion shown and amount extraction.
  // If `extracted_amount` here is a nullptr, it implies the amount extraction
  // result is never received.
  if (!suggestions_shown_response || !extracted_amount) {
    // No need to update the suggestions if the function is called with partial
    // input.
    // This is not a common case and only happens when amount extraction is not
    // completed and a second suggestion show is triggered without amount
    // extraction.
    return;
  }

  // If `extracted_amount` here is a nullopt, it implies an amount extraction
  // result is received but the extraction is a failure.
  if (!extracted_amount->has_value()) {
    // No need to update the suggestions if the extracted amount is not in
    // correct format or empty.
    return;
  }

  const std::vector<BnplIssuer>& bnpl_issuers =
      payments_autofill_client_->GetPaymentsDataManager().GetBnplIssuers();

  if (std::none_of(bnpl_issuers.begin(), bnpl_issuers.end(),
                   [extracted_amount](const BnplIssuer& bnpl_issuer) {
                     return ShouldShowBnplOptionForIssuer(
                         bnpl_issuer, extracted_amount->value());
                   })) {
    // If the extracted amount is not supported by any issuer, no need to update
    // the suggestion list.
    return;
  }

  // Append the BNPL suggestion at the end of the existing suggestion list
  // (before footer items).
  BnplSuggestionUpdateResult update_suggestions_result =
      ::autofill::MaybeUpdateSuggestionsWithBnpl(
          /*current_suggestions=*/std::get<0>(*suggestions_shown_response),
          bnpl_issuers);

  if (!update_suggestions_result.is_bnpl_suggestion_added) {
    // No need to update the pop up, if no BNPL suggestion is added.
    return;
  }

  // Update the pop up with BNPL suggestion entry added to the current shown
  // suggestion list.
  std::get<1>(*suggestions_shown_response)
      .Run(update_suggestions_result.suggestions, trigger_source);
}

std::set<std::string> BnplManager::GetBnplSupportedCountries() {
  return {"US"};
}

bool BnplManager::ShouldShowBnplSettingsToggle() const {
  return !payments_autofill_client_->GetPaymentsDataManager()
              .GetBnplIssuers()
              .empty() &&
         (base::FeatureList::IsEnabled(
              features::kAutofillEnableBuyNowPayLaterForAffirm) ||
          base::FeatureList::IsEnabled(
              features::kAutofillEnableBuyNowPayLaterForZip));
}

}  // namespace autofill::payments
