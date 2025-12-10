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
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_strategy.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_payments_features.h"

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#elif BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

namespace autofill::payments {

namespace {

// Returns true if the `extracted_amount_in_micros` is supported by any of the
// `bnpl_issuers`.
bool IsExtractedAmountSupportedByAnyBnplIssuer(
    const std::vector<BnplIssuer>& bnpl_issuers,
    std::optional<int64_t> extracted_amount_in_micros) {
  if (!extracted_amount_in_micros) {
    return false;
  }
  return std::any_of(
      bnpl_issuers.begin(), bnpl_issuers.end(),
      [extracted_amount_in_micros](const BnplIssuer& bnpl_issuer) {
        // For MVP, BNPL will only target US users and support
        // USD.
        return bnpl_issuer.IsEligibleAmount(extracted_amount_in_micros.value(),
                                            /*currency=*/"USD");
      });
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
bool BnplManager::IsBnplIssuerSupported(std::string_view issuer_id) {
  base::flat_set<std::string_view> supported_issuers = {
      autofill::ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplAffirm),
      autofill::ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplZip)};

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableBuyNowPayLaterForKlarna)) {
    supported_issuers.insert(autofill::ConvertToBnplIssuerIdString(
        BnplIssuer::IssuerId::kBnplKlarna));
  }

  return supported_issuers.contains(issuer_id);
}

void BnplManager::OnDidAcceptBnplSuggestion(
    std::optional<int64_t> final_checkout_amount,
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback) {
  ongoing_flow_state_ = std::make_unique<OngoingFlowState>();
  ongoing_flow_state_->final_checkout_amount = std::move(final_checkout_amount);
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

  CHECK(payments_autofill_client().GetBnplStrategy());
  using enum BnplStrategy::BnplSuggestionAcceptedNextAction;
  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetNextActionOnBnplSuggestionAcceptance()) {
    case kShowSelectBnplIssuerUi: {
      CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
          .ShowSelectBnplIssuerUi(
              GetSortedBnplIssuerContext(), ongoing_flow_state_->app_locale,
              base::BindOnce(&BnplManager::OnIssuerSelected,
                             weak_factory_.GetWeakPtr()),
              base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()),
              HasSeenAmountExtractionAiTerms());

      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableAiBasedAmountExtraction)) {
        if (HasSeenAmountExtractionAiTerms()) {
          // On BNPL suggestion acceptance, if the user has seen the AI terms,
          // server-side amount extraction call should be made directly.
          browser_autofill_manager_->GetAmountExtractionManager()
              .TriggerCheckoutAmountExtractionWithAi();
        } else {
          // On BNPL suggestion acceptance, if the user has not seen the AI
          // terms, record the user has seen the AI terms after the dialog has
          // been shown.
          payments_autofill_client()
              .GetPaymentsDataManager()
              .SetAutofillAmountExtractionAiTermsSeen();
        }
      }
      break;
    }
    case kCheckAmountExtractionBeforeContinuingFlow: {
      base::OnceClosure cancel_callback;
#if BUILDFLAG(IS_ANDROID)
      cancel_callback =
          base::BindOnce(&BnplManager::OnTouchToFillIssuerSelectionCancelled,
                         weak_factory_.GetWeakPtr());
#else
      cancel_callback =
          base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr());
#endif  // BUILDFLAG(IS_ANDROID)
      // Shows the issuer selection screen when amount extraction returns a
      // valid amount.
      if (ongoing_flow_state_->final_checkout_amount.has_value()) {
        CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
            .ShowSelectBnplIssuerUi(
                GetSortedBnplIssuerContext(), ongoing_flow_state_->app_locale,
                base::BindOnce(&BnplManager::OnIssuerSelected,
                               weak_factory_.GetWeakPtr()),
                std::move(cancel_callback), HasSeenAmountExtractionAiTerms());
      } else {
        // We can only enter this branch if the user selected the BNPL chip
        // before amount extraction returned. If amount extraction had already
        // completed without a final checkout amount, the chip would be in a
        // disabled state and not clickable.
        CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
            .ShowProgressUi(
                AutofillProgressDialogType::kBnplAmountExtractionProgressUi,
                /*cancel_callback=*/base::BindOnce(&BnplManager::Reset,
                                                   weak_factory_.GetWeakPtr()));
      }
      break;
    }
  }

  browser_autofill_manager_->GetCreditCardFormEventLogger()
      .OnDidAcceptBnplSuggestion();
}

void BnplManager::NotifyOfSuggestionGeneration(
    const AutofillSuggestionTriggerSource trigger_source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableBuyNowPayLater)) {
    return;
  }

  update_suggestions_barrier_callback_ = base::BarrierCallback<
      std::variant<SuggestionsShownResponse, std::optional<int64_t>>>(
      2U, base::BindOnce(&BnplManager::MaybeUpdateDesktopSuggestionsWithBnpl,
                         weak_factory_.GetWeakPtr(), trigger_source));
}

void BnplManager::OnSuggestionsShown(
    base::span<const Suggestion> suggestions,
    UpdateSuggestionsCallback update_suggestions_callback) {
  if (!update_suggestions_barrier_callback_.has_value()) {
    return;
  }

  CHECK(payments_autofill_client().GetBnplStrategy());
  using enum BnplStrategy::SuggestionShownNextAction;
  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetNextActionOnSuggestionShown()) {
    case kNotifyUpdateCallbackOfSuggestionsShownResponse:
      // The update suggestions callback attempts to add a BNPL entry to the
      // list of suggestions if no BNPL entry exists in the list.
      if (!base::Contains(suggestions, SuggestionType::kBnplEntry,
                          &Suggestion::type)) {
        update_suggestions_barrier_callback_->Run(SuggestionsShownResponse(
            std::vector<Suggestion>(std::begin(suggestions),
                                    std::end(suggestions)),
            std::move(update_suggestions_callback)));
      }
      break;
    case kSkipNotifyingUpdateCallbackOfSuggestionsShownResponse:
      break;
  }
}

void BnplManager::OnAmountExtractionReturned(
    const std::optional<int64_t>& extracted_amount,
    bool timeout_reached) {
  bool is_amount_supported_by_any_issuer =
      IsExtractedAmountSupportedByAnyBnplIssuer(
          payments_autofill_client().GetPaymentsDataManager().GetBnplIssuers(),
          extracted_amount);
  CHECK(payments_autofill_client().GetBnplStrategy());
  using enum BnplStrategy::BnplAmountExtractionReturnedNextAction;
  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetNextActionOnAmountExtractionReturned()) {
    case kNotifyUpdateCallbackOfAmountExtractionReturnedResponse:
      if (!update_suggestions_barrier_callback_.has_value()) {
        return;
      }
      update_suggestions_barrier_callback_->Run(extracted_amount);
      break;
    case kNotifyUiOfAmountExtractionReturnedResponse:
      // `ongoing_flow_state_` being present indicates the user accepted the
      //  BNPL suggestion.
      if (ongoing_flow_state_) {
        // Notify the UI when amount extraction completes while the progress
        // screen is currently visible following the BNPL chip click. The UI
        // will then update its state based on the result of amount extraction.
        ongoing_flow_state_->final_checkout_amount = extracted_amount;
        // TODO(crbug.com/438784412): Handle cases where the amount is present
        // but unsupported by all issuers, or if the amount is null.
        payments_autofill_client().OnPurchaseAmountExtracted(
            extracted_amount.has_value() ? GetSortedBnplIssuerContext()
                                         : std::vector<BnplIssuerContext>(),
            extracted_amount, is_amount_supported_by_any_issuer,
            ongoing_flow_state_->app_locale,
            base::BindOnce(&BnplManager::OnIssuerSelected,
                           weak_factory_.GetWeakPtr()),
            base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));

      } else {
        // Notify the UI when amount extraction completes while BNPL payment
        // method is visible. The UI will then update the BNPL chip based on
        // the result of amount extraction.
        // TODO(crbug.com/438784412): When the amount exists, pass the BNPL
        // issuer context instead of an empty list. The Java side should cache
        // this context, allowing us to remove the bnpl issuer context
        // parameter from the showBnplIssuer method.
        payments_autofill_client().OnPurchaseAmountExtracted(
            /*bnpl_issuer_contexts=*/std::vector<BnplIssuerContext>(),
            extracted_amount, is_amount_supported_by_any_issuer,
            /*app_locale=*/std::nullopt,
            /*selected_issuer_callback=*/base::DoNothing(),
            /*cancel_callback=*/base::DoNothing());
      }
      break;
  }
  if (!has_logged_bnpl_suggestion_unavailable_reason_) {
    if (timeout_reached) {
      LogBnplSuggestionUnavailableReason(
          autofill_metrics::BnplSuggestionUnavailableReason::
              kAmountExtractionTimeout);
      has_logged_bnpl_suggestion_unavailable_reason_ = true;
    } else if (!extracted_amount) {
      LogBnplSuggestionUnavailableReason(
          autofill_metrics::BnplSuggestionUnavailableReason::
              kAmountExtractionFailure);
      has_logged_bnpl_suggestion_unavailable_reason_ = true;
    } else if (!is_amount_supported_by_any_issuer) {
      LogBnplSuggestionUnavailableReason(
          autofill_metrics::BnplSuggestionUnavailableReason::
              kCheckoutAmountNotSupported);
      has_logged_bnpl_suggestion_unavailable_reason_ = true;
    }
  }
}

void BnplManager::OnAmountExtractionReturnedFromAi(
    const std::optional<int64_t>& extracted_amount_in_micros,
    bool timeout_reached) {
  if (timeout_reached || !extracted_amount_in_micros.has_value()) {
    // If an invalid response is received from `AmountExtractionManager`, BNPL
    // flow will close the current dialog and show the error dialog.
    payments_autofill_client()
        .GetBnplUiDelegate()
        ->RemoveSelectBnplIssuerOrProgressUi();

    payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/false));

    Reset();
    return;
  }

  ongoing_flow_state_->final_checkout_amount = extracted_amount_in_micros;

  if (IssuerSelectedAndCheckoutAmountWithinRange()) {
    // If the selected issuer is eligible, continue the BNPL flow with this
    // issuer.
    OnIssuerSelectedAndCheckoutAmountAvailable();
  } else {
    // If the selected issuer is not eligible, update UI.
    // This handles both "issuer selected but ineligible" and
    // "no issuer selected yet" cases.
    CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
        .UpdateBnplIssuerDialogUi(GetSortedBnplIssuerContext());
  }
}

bool BnplManager::AcceptTosActionRequired() const {
  return ongoing_flow_state_->issuer->payment_instrument().has_value() &&
         base::Contains(ongoing_flow_state_->issuer->payment_instrument()
                            ->action_required(),
                        PaymentInstrument::ActionRequired::kAcceptTos);
}

bool BnplManager::HasSeenAmountExtractionAiTerms() const {
  return payments_autofill_client()
      .GetPaymentsDataManager()
      .IsAutofillAmountExtractionAiTermsSeenPrefEnabled();
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
      ongoing_flow_state_->issuer->issuer_id());

  CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
      .ShowProgressUi(AutofillProgressDialogType::kBnplFetchVcnProgressDialog,
                      /*cancel_callback=*/base::BindOnce(
                          [](base::WeakPtr<BnplManager> manager) {
                            if (manager) {
                              // Note: Does not call
                              // `BnplUiDelegate::CloseProgressUi()` as this is
                              // expected to be handled by UI code.
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

  CHECK(payments_autofill_client().GetBnplUiDelegate());
  CHECK(payments_autofill_client().GetBnplStrategy());
  if (payments_autofill_client()
          .GetBnplStrategy()
          ->ShouldRemoveExistingUiOnServerReturn(result)) {
    payments_autofill_client().GetBnplUiDelegate()->CloseProgressUi(
        /*credit_card_fetched_successfully=*/successful);
  }

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
        ongoing_flow_state_->issuer->issuer_id()));
    credit_card.set_is_bnpl_card(true);
    credit_card.SetNickname(ongoing_flow_state_->issuer->GetDisplayName());
    credit_card.set_server_id(ongoing_flow_state_->instrument_id);
    std::move(ongoing_flow_state_->on_bnpl_vcn_fetched_callback)
        .Run(credit_card);
  } else {
    payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
  }
  Reset();
}

void BnplManager::OnIssuerSelected(BnplIssuer selected_issuer) {
  ongoing_flow_state_->issuer = std::move(selected_issuer);

  // TODO(crbug.com/424259928): Refactor BnplManager to always use
  // `ongoing_flow_state_->instrument_id` instead of
  // `ongoing_flow_state_->issuer->payment_instrument()->instrument_id()` to
  // prevent duplicity.
  bool is_linked_issuer =
      ongoing_flow_state_->issuer->payment_instrument().has_value();
  if (is_linked_issuer) {
    ongoing_flow_state_->instrument_id = base::NumberToString(
        ongoing_flow_state_->issuer->payment_instrument()->instrument_id());
  }

  // When an issuer is selected but amount is not received, call server-side AI
  // to extract the amount.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction) &&
      !ongoing_flow_state_->final_checkout_amount) {
    browser_autofill_manager_->GetAmountExtractionManager()
        .TriggerCheckoutAmountExtractionWithAi();
    return;
  }

  OnIssuerSelectedAndCheckoutAmountAvailable();
}

bool BnplManager::IssuerSelectedAndCheckoutAmountWithinRange() {
  // Check eligibility if an issuer was selected.
  if (!ongoing_flow_state_->issuer) {
    return false;
  }

  // TODO(crbug.com/444684996): Pass in currency from
  // `AmountExtractionManager`.
  const base::optional_ref<const BnplIssuer::EligiblePriceRange> price_range =
      ongoing_flow_state_->issuer->GetEligiblePriceRangeForCurrency("USD");
  CHECK(price_range.has_value());
  if (ongoing_flow_state_->final_checkout_amount <
          price_range->price_lower_bound ||
      ongoing_flow_state_->final_checkout_amount >
          price_range->price_upper_bound) {
    return false;
  }
  return true;
}

void BnplManager::OnIssuerSelectedAndCheckoutAmountAvailable() {
  bool is_linked_issuer =
      ongoing_flow_state_->issuer->payment_instrument().has_value();
  if (is_linked_issuer && !AcceptTosActionRequired()) {
    LoadRiskDataForFetchingRedirectUrl();
  } else {
    GetLegalMessageFromServer();
  }
}

void BnplManager::GetLegalMessageFromServer() {
  if (AcceptTosActionRequired()) {
    GetDetailsForUpdateBnplPaymentInstrument();
  } else {
    GetDetailsForCreateBnplPaymentInstrument();
  }
}

void BnplManager::GetDetailsForCreateBnplPaymentInstrument() {
  GetDetailsForCreateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
#if BUILDFLAG(IS_ANDROID)
  request_details.client_behavior_signals.push_back(
      ClientBehaviorConstants::kShowAccountEmailInLegalMessage);
#endif  // BUILDFLAG(IS_ANDROID)
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer->issuer_id());

  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->GetDetailsForCreateBnplPaymentInstrument(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnDidGetLegalMessageFromServer,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::GetDetailsForUpdateBnplPaymentInstrument() {
  GetDetailsForUpdateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
#if BUILDFLAG(IS_ANDROID)
  request_details.client_behavior_signals.push_back(
      ClientBehaviorConstants::kShowAccountEmailInLegalMessage);
#endif  // BUILDFLAG(IS_ANDROID)
  request_details.instrument_id =
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id();
  request_details.type =
      GetDetailsForUpdateBnplPaymentInstrumentRequestDetails::
          GetDetailsForUpdateBnplPaymentInstrumentType::kGetDetailsForAcceptTos;
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer->issuer_id());
  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->GetDetailsForUpdateBnplPaymentInstrument(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnDidGetLegalMessageFromServer,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnDidGetLegalMessageFromServer(
    PaymentsAutofillClient::PaymentsRpcResult result,
    std::string context_token,
    LegalMessageLines legal_message) {
  // Dismiss the loading throbber in the issuer selection UI or progress
  // throbber UI after the server call completion to show the next UI.
  CHECK(payments_autofill_client().GetBnplUiDelegate());
  CHECK(payments_autofill_client().GetBnplStrategy());
  using enum BnplStrategy::BeforeSwitchingViewAction;

  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetBeforeViewSwitchAction()) {
    case kDoNothing:
      // The `kDoNothing` case is for platforms where the view is flipped to the
      // ToS or error UI within the same view, so removing it is not necessary.
      break;
    case kCloseCurrentUi:
      payments_autofill_client()
          .GetBnplUiDelegate()
          ->RemoveSelectBnplIssuerOrProgressUi();
  }

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->context_token = std::move(context_token);

    CHECK(!legal_message.empty());
    BnplTosModel bnpl_tos_model;
    bnpl_tos_model.legal_message_lines = std::move(legal_message);
    bnpl_tos_model.issuer = ongoing_flow_state_->issuer.value();

    payments_autofill_client().GetBnplUiDelegate()->ShowBnplTosUi(
        std::move(bnpl_tos_model),
        base::BindOnce(&BnplManager::OnTosDialogAccepted,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));
    return;
  }

  payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
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

void BnplManager::OnFailureAfterTosAccepted(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  CHECK(payments_autofill_client().GetBnplUiDelegate());
  CHECK(payments_autofill_client().GetBnplStrategy());
  using enum BnplStrategy::BeforeSwitchingViewAction;

  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetBeforeViewSwitchAction()) {
    // This case is for platforms (i.e. Android) that will flip to the error
    // screen within the same view, so no need to remove the current view.
    case kDoNothing:
      break;
    case kCloseCurrentUi:
      payments_autofill_client()
          .GetBnplUiDelegate()
          ->RemoveBnplTosOrProgressUi();
  }

  payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
      AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
          /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
  Reset();
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
  CHECK(ongoing_flow_state_->final_checkout_amount);
  request_details.total_amount =
      ongoing_flow_state_->final_checkout_amount.value();
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
  CHECK(payments_autofill_client().GetBnplUiDelegate());
  CHECK(payments_autofill_client().GetBnplStrategy());
  if (payments_autofill_client()
          .GetBnplStrategy()
          ->ShouldRemoveExistingUiOnServerReturn(result)) {
    if (ongoing_flow_state_->issuer->payment_instrument().has_value() &&
        !AcceptTosActionRequired()) {
      // If the BNPL issuer selected is linked and doesn't require ToS
      // acceptance, then the issuer selection UI or progress UI must be
      // showing, so close it.
      payments_autofill_client()
          .GetBnplUiDelegate()
          ->RemoveSelectBnplIssuerOrProgressUi();
    } else {
      // If the BNPL issuer selected is unlinked, or is linked but requires ToS
      // acceptance, then the ToS/progress UI must be showing, so remove it.
      payments_autofill_client()
          .GetBnplUiDelegate()
          ->RemoveBnplTosOrProgressUi();
    }
  }

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->redirect_url = std::move(response.redirect_url);
    ongoing_flow_state_->context_token = std::move(response.context_token);

    PaymentsWindowManager::BnplContext payments_window_bnpl_context;
    payments_window_bnpl_context.issuer_id =
        ongoing_flow_state_->issuer->issuer_id();
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
    payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
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
      CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
          .ShowAutofillErrorUi(
              AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                  /*is_permanent_error=*/false));
      Reset();
      break;
  }
}

void BnplManager::MaybeUpdateDesktopSuggestionsWithBnpl(
    const AutofillSuggestionTriggerSource trigger_source,
    std::vector<std::variant<SuggestionsShownResponse, std::optional<int64_t>>>
        responses) {
  update_suggestions_barrier_callback_ = std::nullopt;

  SuggestionsShownResponse* suggestions_shown_response = nullptr;
  std::optional<int64_t>* extracted_amount = nullptr;
  for (auto& response : responses) {
    if (std::holds_alternative<SuggestionsShownResponse>(response)) {
      suggestions_shown_response =
          std::get_if<SuggestionsShownResponse>(&response);
    } else {
      extracted_amount = std::get_if<std::optional<int64_t>>(&response);
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

  std::vector<BnplIssuer> bnpl_issuers =
      payments_autofill_client().GetPaymentsDataManager().GetBnplIssuers();

  if (!IsExtractedAmountSupportedByAnyBnplIssuer(bnpl_issuers,
                                                 *extracted_amount)) {
    // If the extracted amount is not supported by any issuer, no need to update
    // the suggestion list.
    return;
  }

  // Append the BNPL suggestion at the end of the existing suggestion list
  // (before footer items).
  BnplSuggestionUpdateResult update_suggestions_result =
      ::autofill::MaybeUpdateDesktopSuggestionsWithBnpl(
          /*current_suggestions=*/std::get<0>(*suggestions_shown_response),
          std::move(bnpl_issuers), extracted_amount->value());

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
    if (AcceptTosActionRequired()) {
      UpdateBnplPaymentInstrument();
    } else {
      CreateBnplPaymentInstrument();
    }
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
  if (AcceptTosActionRequired()) {
    UpdateBnplPaymentInstrument();
  } else {
    CreateBnplPaymentInstrument();
  }
}

void BnplManager::CreateBnplPaymentInstrument() {
  CreateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer->issuer_id());
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
    OnFailureAfterTosAccepted(result);
  }
}

void BnplManager::UpdateBnplPaymentInstrument() {
  UpdateBnplPaymentInstrumentRequestDetails request_details;
  request_details.app_locale = ongoing_flow_state_->app_locale;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer->issuer_id());
  request_details.instrument_id =
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id();
  request_details.risk_data = ongoing_flow_state_->risk_data;
  request_details.type = UpdateBnplPaymentInstrumentRequestDetails::
      UpdateBnplPaymentInstrumentType::kAcceptTos;
  payments_autofill_client()
      .GetPaymentsNetworkInterface()
      ->UpdateBnplPaymentInstrument(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnBnplPaymentInstrumentUpdated,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnBnplPaymentInstrumentUpdated(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    FetchRedirectUrl();
  } else {
    OnFailureAfterTosAccepted(result);
  }
}

std::vector<BnplIssuerContext> BnplManager::GetSortedBnplIssuerContext() {
  AutofillOptimizationGuideDecider* autofill_optimization_guide =
      browser_autofill_manager_->client().GetAutofillOptimizationGuideDecider();
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
        } else if (!ongoing_flow_state_->final_checkout_amount) {
          // The only case this code gets hit is `BnplManager` needs to build
          // the issuer view before the LLM call returns a valid checkout
          // amount.
          eligibility = BnplIssuerEligibilityForPage::
              kTemporarilyEligibleCheckoutAmountNotYetKnown;
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

#if BUILDFLAG(IS_ANDROID)
void BnplManager::OnTouchToFillIssuerSelectionCancelled() {
  // TODO(crbug.com/430575808): Add a metric to track cancellations on the
  // selection screen.
  Reset();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill::payments
