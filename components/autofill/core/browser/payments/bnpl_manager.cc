// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

namespace {

// Returns true if the `extracted_amount_in_micros` is supported by
// `bnpl_issuer`.
bool ShouldShowBnplOptionForIssuer(const BnplIssuer& bnpl_issuer,
                                   uint64_t extracted_amount_in_micros) {
  // For MVP, BNPL will only target US users and support USD.
  return bnpl_issuer.IsEligibleAmount(extracted_amount_in_micros,
                                      /*currency=*/"USD");
}

bool ShouldShowPermanentErrorDialog(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  return result == PaymentsAutofillClient::PaymentsRpcResult::
                       kVcnRetrievalPermanentFailure;
}

}  // namespace

BnplManager::OngoingFlowState::OngoingFlowState() = default;

BnplManager::OngoingFlowState::~OngoingFlowState() = default;

BnplManager::BnplManager(BrowserAutofillManager* browser_autofill_manager)
    : browser_autofill_manager_(CHECK_DEREF(browser_autofill_manager)) {}

BnplManager::~BnplManager() = default;

// static
const std::array<std::string_view, 2>&
BnplManager::GetSupportedBnplIssuerIds() {
  // Calling `ConvertToBnplIssuerIdString` serves as a validation step,
  // verifying that each supported Bnpl IssuerId enum value has a corresponding
  // string representation. This helps maintain the invariant with
  // `ConvertToBnplIssuerIdEnum`.
  static const std::array<std::string_view, 2> kBnplIssuers = {
      autofill::ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplAffirm),
      autofill::ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplZip)};
  return kBnplIssuers;
}

void BnplManager::OnDidAcceptBnplSuggestion(
    uint64_t final_checkout_amount,
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback) {
  ongoing_flow_state_ = std::make_unique<OngoingFlowState>();

  ongoing_flow_state_->final_checkout_amount = final_checkout_amount;
  ongoing_flow_state_->app_locale =
      browser_autofill_manager_->client().GetAppLocale();
  ongoing_flow_state_->billing_customer_number =
      GetBillingCustomerId(payments_autofill_client().GetPaymentsDataManager());
  ongoing_flow_state_->on_bnpl_vcn_fetched_callback =
      std::move(on_bnpl_vcn_fetched_callback);

  // Prefetch risk data to improve flow latency by reducing the need to fetch
  // risk data later, as it can take several seconds in some rare cases.
  payments_autofill_client().LoadRiskData(base::BindOnce(
      &BnplManager::OnPrefetchedRiskDataLoaded, weak_factory_.GetWeakPtr()));

  payments_autofill_client().ShowSelectBnplIssuerDialog(
      GetSortedBnplIssuerContext(), ongoing_flow_state_->app_locale,
      base::BindOnce(&BnplManager::OnIssuerSelected,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));

  browser_autofill_manager_->GetCreditCardFormEventLogger()
      .OnDidAcceptBnplSuggestion();
}

void BnplManager::NotifyOfSuggestionGeneration(
    const AutofillSuggestionTriggerSource trigger_source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableBuyNowPayLater)) {
    return;
  }

  update_suggestions_barrier_callback_ = base::BarrierCallback<
      std::variant<SuggestionsShownResponse, std::optional<uint64_t>>>(
      2U, base::BindOnce(&BnplManager::MaybeUpdateSuggestionsWithBnpl,
                         weak_factory_.GetWeakPtr(), trigger_source));
}

void BnplManager::OnSuggestionsShown(
    base::span<const Suggestion> suggestions,
    UpdateSuggestionsCallback update_suggestions_callback) {
  if (!update_suggestions_barrier_callback_.has_value()) {
    return;
  }

  // Do not proceed to calling the barrier callback, if the suggestion list
  // already contains a buy-now-pay-later-entry (which is triggered after
  // updating the original suggestion list).
  if (base::Contains(suggestions, SuggestionType::kBnplEntry,
                     &Suggestion::type)) {
    return;
  }

  if (update_suggestions_barrier_callback_.has_value()) {
    update_suggestions_barrier_callback_->Run(SuggestionsShownResponse(
        std::vector<Suggestion>(std::begin(suggestions), std::end(suggestions)),
        std::move(update_suggestions_callback)));
  }
}

void BnplManager::OnAmountExtractionReturned(
    const std::optional<uint64_t>& extracted_amount) {
  if (!update_suggestions_barrier_callback_.has_value()) {
    return;
  }

  if (update_suggestions_barrier_callback_.has_value()) {
    update_suggestions_barrier_callback_->Run(extracted_amount);
  }

  if (!extracted_amount && !has_logged_bnpl_suggestion_not_shown_reason_) {
    LogBnplSuggestionNotShownReason(
        autofill_metrics::BnplSuggestionNotShownReason::
            kAmountExtractionFailure);
    has_logged_bnpl_suggestion_not_shown_reason_ = true;
  }
}

void BnplManager::FetchVcnDetails(GURL url) {
  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.instrument_id = ongoing_flow_state_->instrument_id;
  request_details.risk_data = ongoing_flow_state_->risk_data;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.redirect_url = std::move(url);
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer.issuer_id());

  payments_autofill_client().ShowAutofillProgressDialog(
      AutofillProgressDialogType::kBnplFetchVcnProgressDialog,
      /*cancel_callback=*/base::BindOnce(
          [](base::WeakPtr<BnplManager> manager) {
            if (manager) {
              // Note: Does not call
              // `PaymentsAutofillClient::CloseAutofillProgressDialog()` as this
              // is expected to be handled by dialog UI code.
              manager->Reset();
            }
          },
          weak_factory_.GetWeakPtr()));

  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->GetBnplPaymentInstrumentForFetchingVcn(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnVcnDetailsFetched,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::Reset() {
  payments_autofill_client().GetPaymentsNetworkInterface()->CancelRequest();
  ongoing_flow_state_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void BnplManager::OnVcnDetailsFetched(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const BnplFetchVcnResponseDetails& response_details) {
  bool successful =
      result == PaymentsAutofillClient::PaymentsRpcResult::kSuccess;

  payments_autofill_client().CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/successful,
      /*no_interactive_authentication_callback=*/base::OnceClosure());

  if (successful) {
    CHECK(ongoing_flow_state_);
    CreditCard credit_card;
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::UTF8ToUTF16(response_details.pan));
    credit_card.set_record_type(CreditCard::RecordType::kVirtualCard);
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::UTF8ToUTF16(response_details.cardholder_name));
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_EXP_MONTH,
        base::UTF8ToUTF16(response_details.expiration_month));
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                           base::UTF8ToUTF16(response_details.expiration_year));
    credit_card.set_cvc(base::UTF8ToUTF16(response_details.cvv));
    credit_card.set_issuer_id(autofill::ConvertToBnplIssuerIdString(
        ongoing_flow_state_->issuer.issuer_id()));
    credit_card.set_is_bnpl_card(true);
    credit_card.SetNickname(ongoing_flow_state_->issuer.GetDisplayName());
    std::move(ongoing_flow_state_->on_bnpl_vcn_fetched_callback)
        .Run(credit_card);
  } else {
    payments_autofill_client().ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
  }
  Reset();
}

void BnplManager::OnIssuerSelected(BnplIssuer selected_issuer) {
  ongoing_flow_state_->issuer = std::move(selected_issuer);

  if (ongoing_flow_state_->issuer.payment_instrument().has_value()) {
    ongoing_flow_state_->instrument_id = base::NumberToString(
        ongoing_flow_state_->issuer.payment_instrument()->instrument_id());

    LoadRiskDataForFetchingRedirectUrl();
  } else {
    GetDetailsForCreateBnplPaymentInstrument();
  }
}

void BnplManager::GetDetailsForCreateBnplPaymentInstrument() {
  GetDetailsForCreateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer.issuer_id());

  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->GetDetailsForCreateBnplPaymentInstrument(
          std::move(request_details),
          base::BindOnce(
              &BnplManager::OnDidGetDetailsForCreateBnplPaymentInstrument,
              weak_factory_.GetWeakPtr()));
}

void BnplManager::OnDidGetDetailsForCreateBnplPaymentInstrument(
    PaymentsAutofillClient::PaymentsRpcResult result,
    std::string context_token,
    LegalMessageLines legal_message) {
  // Dismiss the loading throbber in the issuer selection dialog after the
  // server call completion to show the next dialog.
  payments_autofill_client().DismissSelectBnplIssuerDialog();

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->context_token = std::move(context_token);

    CHECK(!legal_message.empty());
    BnplTosModel bnpl_tos_model;
    bnpl_tos_model.legal_message_lines = std::move(legal_message);
    bnpl_tos_model.issuer = ongoing_flow_state_->issuer;

    payments_autofill_client().ShowBnplTos(
        std::move(bnpl_tos_model),
        base::BindOnce(&BnplManager::OnTosDialogAccepted,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));
    return;
  }

  payments_autofill_client().ShowAutofillErrorDialog(
      AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
          /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));

  Reset();
}

void BnplManager::LoadRiskDataForFetchingRedirectUrl() {
  if (ongoing_flow_state_->risk_data.empty()) {
    payments_autofill_client().LoadRiskData(base::BindOnce(
        &BnplManager::OnRiskDataLoadedAfterIssuerSelectionDialogAcceptance,
        weak_factory_.GetWeakPtr()));
    return;
  }

  FetchRedirectUrl();
}

void BnplManager::OnRiskDataLoadedAfterIssuerSelectionDialogAcceptance(
    const std::string& risk_data) {
  ongoing_flow_state_->risk_data = risk_data;
  FetchRedirectUrl();
}

void BnplManager::FetchRedirectUrl() {
  GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.instrument_id = ongoing_flow_state_->instrument_id;
  request_details.risk_data = ongoing_flow_state_->risk_data;
  request_details.merchant_domain =
      browser_autofill_manager_->client()
          .GetLastCommittedPrimaryMainFrameOrigin()
          .GetURL();
  request_details.total_amount = ongoing_flow_state_->final_checkout_amount;
  // Only `USD` is supported for MVP.
  request_details.currency = "USD";

  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->GetBnplPaymentInstrumentForFetchingUrl(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnRedirectUrlFetched,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnRedirectUrlFetched(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const BnplFetchUrlResponseDetails& response) {
  if (ongoing_flow_state_->issuer.payment_instrument().has_value()) {
    // If the BNPL issuer selected is linked, the issuer selection dialog must
    // be showing, so close it.
    payments_autofill_client().DismissSelectBnplIssuerDialog();
  } else {
    // If the BNPL issuer selected is not linked, the ToS dialog must be
    // showing, so close it.
    payments_autofill_client().CloseBnplTos();
  }

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->redirect_url = std::move(response.redirect_url);
    ongoing_flow_state_->context_token = std::move(response.context_token);

    PaymentsWindowManager::BnplContext payments_window_bnpl_context;
    payments_window_bnpl_context.issuer_id =
        ongoing_flow_state_->issuer.issuer_id();
    payments_window_bnpl_context.initial_url =
        ongoing_flow_state_->redirect_url;
    payments_window_bnpl_context.success_url_prefix =
        std::move(response.success_url_prefix);
    payments_window_bnpl_context.failure_url_prefix =
        std::move(response.failure_url_prefix);
    payments_window_bnpl_context.completion_callback = base::BindOnce(
        &BnplManager::OnPopupWindowCompleted, weak_factory_.GetWeakPtr());

    payments_autofill_client().GetPaymentsWindowManager()->InitBnplFlow(
        std::move(payments_window_bnpl_context));
  } else {
    payments_autofill_client().ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
    Reset();
  }
}

void BnplManager::OnPopupWindowCompleted(
    PaymentsWindowManager::BnplFlowResult result,
    GURL url) {
  switch (result) {
    case PaymentsWindowManager::BnplFlowResult::kUserClosed:
      Reset();
      break;
    case PaymentsWindowManager::BnplFlowResult::kSuccess:
      FetchVcnDetails(std::move(url));
      break;
    case PaymentsWindowManager::BnplFlowResult::kFailure:
      payments_autofill_client().ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      Reset();
      break;
  }
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
      payments_autofill_client().GetPaymentsDataManager().GetBnplIssuers();

  if (std::none_of(bnpl_issuers.begin(), bnpl_issuers.end(),
                   [extracted_amount](const BnplIssuer& bnpl_issuer) {
                     return ShouldShowBnplOptionForIssuer(
                         bnpl_issuer, extracted_amount->value());
                   })) {
    // If the extracted amount is not supported by any issuer, no need to update
    // the suggestion list.
    if (!has_logged_bnpl_suggestion_not_shown_reason_) {
      LogBnplSuggestionNotShownReason(
          autofill_metrics::BnplSuggestionNotShownReason::
              kCheckoutAmountNotSupported);
      has_logged_bnpl_suggestion_not_shown_reason_ = true;
    }
    return;
  }

  // Append the BNPL suggestion at the end of the existing suggestion list
  // (before footer items).
  BnplSuggestionUpdateResult update_suggestions_result =
      ::autofill::MaybeUpdateSuggestionsWithBnpl(
          /*current_suggestions=*/std::get<0>(*suggestions_shown_response),
          bnpl_issuers, extracted_amount->value());

  if (!update_suggestions_result.is_bnpl_suggestion_added) {
    // No need to update the pop up, if no BNPL suggestion is added.
    return;
  }

  // Update the pop up with BNPL suggestion entry added to the current shown
  // suggestion list.
  std::get<1>(*suggestions_shown_response)
      .Run(update_suggestions_result.suggestions, trigger_source);
  browser_autofill_manager_->GetCreditCardFormEventLogger()
      .OnBnplSuggestionShown();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  payments_autofill_client().GetPaymentsDataManager().SetAutofillHasSeenBnpl();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

void BnplManager::OnTosDialogAccepted() {
  if (!ongoing_flow_state_->risk_data.empty()) {
    CreateBnplPaymentInstrument();
    return;
  }

  payments_autofill_client().LoadRiskData(
      base::BindOnce(&BnplManager::OnRiskDataLoadedAfterTosDialogAcceptance,
                     weak_factory_.GetWeakPtr()));
}

void BnplManager::OnPrefetchedRiskDataLoaded(const std::string& risk_data) {
  ongoing_flow_state_->risk_data = risk_data;
}

void BnplManager::OnRiskDataLoadedAfterTosDialogAcceptance(
    const std::string& risk_data) {
  ongoing_flow_state_->risk_data = risk_data;
  CreateBnplPaymentInstrument();
}

void BnplManager::CreateBnplPaymentInstrument() {
  CreateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer.issuer_id());
  request_details.risk_data = ongoing_flow_state_->risk_data;
  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->CreateBnplPaymentInstrument(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnBnplPaymentInstrumentCreated,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnBnplPaymentInstrumentCreated(
    PaymentsAutofillClient::PaymentsRpcResult result,
    std::string instrument_id) {
  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->instrument_id = std::move(instrument_id);
    FetchRedirectUrl();
  } else {
    payments_autofill_client().CloseBnplTos();
    payments_autofill_client().ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
    Reset();
  }
}

std::vector<BnplIssuerContext> BnplManager::GetSortedBnplIssuerContext() {
  AutofillOptimizationGuide* autofill_optimization_guide =
      browser_autofill_manager_->client().GetAutofillOptimizationGuide();
  const GURL& merchant_url = browser_autofill_manager_->client()
                                 .GetLastCommittedPrimaryMainFrameOrigin()
                                 .GetURL();

  // Check BNPL issuer eligibility for the current page and save the
  // eligibility with the corresponding issuer to the vector of
  // `BnplIssuerContext`.
  std::vector<BnplIssuerContext> result = base::ToVector(
      payments_autofill_client().GetPaymentsDataManager().GetBnplIssuers(),
      [this, &autofill_optimization_guide,
       &merchant_url](const BnplIssuer& issuer) -> BnplIssuerContext {
        // For MVP, BNPL will only target US users and support USD.
        const base::optional_ref<const BnplIssuer::EligiblePriceRange>
            price_range =
                issuer.GetEligiblePriceRangeForCurrency(/*currency=*/"USD");
        CHECK(price_range.has_value());

        BnplIssuerEligibilityForPage eligibility;

        if (!autofill_optimization_guide->IsUrlEligibleForBnplIssuer(
                issuer.issuer_id(), merchant_url)) {
          eligibility = BnplIssuerEligibilityForPage::
              kNotEligibleIssuerDoesNotSupportMerchant;
        } else if (ongoing_flow_state_->final_checkout_amount <
                   price_range->price_lower_bound) {
          eligibility =
              BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow;
        } else if (ongoing_flow_state_->final_checkout_amount >
                   price_range->price_upper_bound) {
          eligibility =
              BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh;
        } else {
          eligibility = BnplIssuerEligibilityForPage::kIsEligible;
        }

        return {issuer, eligibility};
      });

  // Shuffle `result` before sorting so that the order of two
  // equivalently-sorted elements are randomized. This is to ensure there is no
  // implicit preference towards any issuers.
  base::RandomShuffle(result.begin(), result.end());

  // Sort the `BnplIssuerContext` vector so that it follows below rules:
  // 1. Eligible issuers should be in front of uneligible ones in a sorted
  //    vector.
  // 2. Linked issuers must go before unlinked ones if they have the same
  //    eligibility.
  // Note: If one issuer has a payment instrument and the other doesn't,
  //    then one is linked and the other is unlinked.
  std::ranges::stable_sort(
      result, [](const BnplIssuerContext& rhs, const BnplIssuerContext& lhs) {
        // Lambda comparator which returns true if `rhs` should be in front of
        // `lhs`.
        // Note: Boolean value `false` is less than boolean value `true`.
        return std::forward_as_tuple(
                   rhs.eligibility == BnplIssuerEligibilityForPage::kIsEligible,
                   rhs.issuer.payment_instrument().has_value()) >
               std::forward_as_tuple(
                   lhs.eligibility == BnplIssuerEligibilityForPage::kIsEligible,
                   lhs.issuer.payment_instrument().has_value());
      });

  return result;
}

bool BnplManager::IsEligibleForBnpl() const {
  AutofillOptimizationGuide* autofill_optimization_guide =
      browser_autofill_manager_->client().GetAutofillOptimizationGuide();
  if (!autofill_optimization_guide) {
    return false;
  }

  const GURL& url =
      browser_autofill_manager_->client().GetLastCommittedPrimaryMainFrameURL();

  return std::ranges::any_of(
      payments_autofill_client().GetPaymentsDataManager().GetBnplIssuers(),
      [&autofill_optimization_guide, &url](const BnplIssuer& bnpl_issuer) {
        return autofill_optimization_guide->IsUrlEligibleForBnplIssuer(
            bnpl_issuer.issuer_id(), url);
      });
}

}  // namespace autofill::payments
