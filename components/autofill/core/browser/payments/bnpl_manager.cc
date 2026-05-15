// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/payment_instrument.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/ai_amount_extraction_metrics.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_strategy.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_ui_type.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/dense_set.h"

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
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
    : browser_autofill_manager_(CHECK_DEREF(browser_autofill_manager)) {
  autofill_manager_observation_.Observe(&*browser_autofill_manager_);
}

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

void BnplManager::OnUserDecisionToUseBnpl(
    std::optional<int64_t> final_checkout_amount,
    OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback) {
  browser_autofill_manager_->GetCreditCardFormEventLogger()
      .OnUserDecisionToUseBnpl();

  if (ongoing_flow_state_ != nullptr &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    // User has already navigated to Pay Later tab before in this popup. This
    // means that either there is an ongoing flow already, or the user is in an
    // error state, both of which mean a new flow should not be started.
    return;
  }

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
  using enum BnplStrategy::UserDecisionToUseBnplNextAction;
  switch (payments_autofill_client()
              .GetBnplStrategy()
              ->GetNextActionOnUserDecisionToUseBnpl()) {
    case kShowSelectBnplIssuerUiForDesktop: {
      CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
          .ShowSelectBnplIssuerUi(
              GetSortedBnplIssuerContext(
                  browser_autofill_manager_->client(),
                  ongoing_flow_state_->final_checkout_amount),
              ongoing_flow_state_->app_locale,
              base::BindRepeating(&BnplManager::OnIssuerAccepted,
                                  weak_factory_.GetWeakPtr()),
              base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()),
              HasSeenAmountExtractionAiTerms());
      break;
    }
    case kCheckAmountExtractionBeforeContinuingFlowForAndroid: {
      // Shows the issuer selection screen when amount extraction returns a
      // valid amount, or when AI-based amount extraction is enabled and the
      // user must see the AI terms before running the extraction for the first
      // time.
      if (ongoing_flow_state_->final_checkout_amount.has_value() ||
          (base::FeatureList::IsEnabled(
               features::kAutofillEnableAiBasedAmountExtraction) &&
           !HasSeenAmountExtractionAiTerms())) {
        CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
            .ShowSelectBnplIssuerUi(
                GetSortedBnplIssuerContext(
                    browser_autofill_manager_->client(),
                    ongoing_flow_state_->final_checkout_amount),
                ongoing_flow_state_->app_locale,
                base::BindRepeating(&BnplManager::OnIssuerAccepted,
                                    weak_factory_.GetWeakPtr()),
                base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()),
                HasSeenAmountExtractionAiTerms());
      } else {
        // We can only enter this branch if the user selected the BNPL chip
        // before amount extraction returned. If amount extraction had already
        // completed without a final checkout amount, the chip would be in a
        // disabled state and not clickable.
        CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
            .ShowProgressUi(
                AutofillProgressUiType::kBnplAmountExtractionProgressUi,
                /*cancel_callback=*/base::BindOnce(&BnplManager::Reset,
                                                   weak_factory_.GetWeakPtr()));
      }
      break;
    }
    case kDoNothing:
      break;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction)) {
    if (HasSeenAmountExtractionAiTerms()) {
      // On user decision to use BNPL, if the user has seen the AI terms,
      // server-side amount extraction call should be made directly.
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnablePayNowPayLaterTabs)) {
        // Do not trigger amount extraction if the card number field is not
        // empty. Instead, continue to show disabled issuers.
        if (is_card_number_field_empty_) {
          browser_autofill_manager_->GetAmountExtractionManager()
              .TriggerCheckoutAmountExtractionWithAi();
        }
      } else {
        browser_autofill_manager_->GetAmountExtractionManager()
            .TriggerCheckoutAmountExtractionWithAi();
      }
    } else {
      // On user decision to use BNPL, if the user has not seen the AI
      // terms, record the user has seen the AI terms after the dialog has
      // been shown.
      payments_autofill_client()
          .GetPaymentsDataManager()
          .SetAutofillAmountExtractionAiTermsSeen();
    }
  }
}

void BnplManager::OnIssuerAccepted(BnplIssuer issuer) {
  ongoing_flow_state_->issuer = std::move(issuer);

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    ReplaceIssuerSuggestionsWithLoadingThrobber();
    if (!has_logged_bnpl_suggestion_accepted_) {
      autofill_metrics::LogPayLaterTabSuggestionAccepted(
          ongoing_flow_state_->issuer->issuer_id(),
          browser_autofill_manager_->driver().GetPageUkmSourceId());
      has_logged_bnpl_suggestion_accepted_ = true;
    }
  }

  // When an issuer is accepted but no checkout amount is present, call
  // server-side AI to extract the amount.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction) &&
      !ongoing_flow_state_->final_checkout_amount) {
    browser_autofill_manager_->GetAmountExtractionManager()
        .TriggerCheckoutAmountExtractionWithAi();
    return;
  }

  OnIssuerAcceptedAndCheckoutAmountAvailable();
}

void BnplManager::NotifyOfSuggestionGeneration(
    const AutofillSuggestionTriggerSource trigger_source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableBuyNowPayLater)) {
    return;
  }

  autofill_suggestion_trigger_source_ = trigger_source;

  // No need to insert BNPL suggestions if the Pay Later tab is enabled.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    return;
  }

  update_suggestions_barrier_callback_ = base::BarrierCallback<
      std::variant<SuggestionsShownResponse, std::optional<int64_t>>>(
      2U, base::BindOnce(&BnplManager::MaybeUpdateDesktopSuggestionsWithBnpl,
                         weak_factory_.GetWeakPtr(), trigger_source));
}

void BnplManager::OnCreditCardSuggestionsShown(
    base::span<const Suggestion> suggestions,
    UpdateSuggestionsCallback update_suggestions_callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    cached_suggestions_ =
        std::vector<Suggestion>(std::begin(suggestions), std::end(suggestions));
  }

  if (std::ranges::contains(suggestions, SuggestionType::kBnplEntry,
                            &Suggestion::type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    payments_autofill_client()
        .GetPaymentsDataManager()
        .SetAutofillHasSeenBnpl();
    browser_autofill_manager_->GetCreditCardFormEventLogger()
        .OnBnplSuggestionShown();
  }

  update_suggestions_callback_ = update_suggestions_callback;

  // Only set `user_has_seen_bnpl_ai_terms_before_` if it has not already been
  // set. This is because `OnCreditCardSuggestionsShown()` may be called
  // again when suggestions are updated.
  if (!user_has_seen_bnpl_ai_terms_before_.has_value()) {
    user_has_seen_bnpl_ai_terms_before_ = HasSeenAmountExtractionAiTerms();
  }

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
      if (!std::ranges::contains(suggestions, SuggestionType::kBnplEntry,
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

void BnplManager::OnUserDecisionToUseSavedCards() {
  CancelOngoingRequests();
  CHECK(ongoing_flow_state_);

  browser_autofill_manager_->GetCreditCardFormEventLogger()
      .OnUserDecisionToUsePayNowTab();

  // Always go to issuer suggestions if there is a checkout amount present.
  // Early return in this case to keep the checkout amount cached.
  if (ongoing_flow_state_->final_checkout_amount) {
    ongoing_flow_state_->issuer.reset();
    ReplaceLoadingThrobberWithIssuerSuggestions(
        GetSortedBnplIssuerContext(browser_autofill_manager_->client(),
                                   ongoing_flow_state_->final_checkout_amount));
    return;
  }

  if (HasSeenAmountExtractionAiTerms() && is_card_number_field_empty_) {
    // Make sure the loading throbber is showing when all below conditions are
    // met:
    // 1. The user has seen the AI terms before.
    // 2. There is no checkout amount retrieved.
    // 3. The card number field is empty.
    ReplaceIssuerSuggestionsWithLoadingThrobber();
  } else {
    // For first time users, if there is no checkout amount, make sure the
    // Pay Later tab is updated to show issuer suggestions.
    ReplaceLoadingThrobberWithIssuerSuggestions(
        GetSortedBnplIssuerContext(browser_autofill_manager_->client(),
                                   ongoing_flow_state_->final_checkout_amount));
  }

  // Reset flow cache to restart the flow if the user select the Pay Later tab
  // again.
  ongoing_flow_state_.reset();
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
        payments_autofill_client().OnPurchaseAmountExtracted(
            extracted_amount.has_value()
                ? GetSortedBnplIssuerContext(
                      browser_autofill_manager_->client(),
                      ongoing_flow_state_->final_checkout_amount)
                : std::vector<BnplIssuerContext>(),
            extracted_amount, is_amount_supported_by_any_issuer,
            ongoing_flow_state_->app_locale,
            base::BindOnce(&BnplManager::OnIssuerAccepted,
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
    const AiAmountExtractionResult::ResultType result) {
  if (!result.has_value()) {
    CHECK(payments_autofill_client().GetBnplUiDelegate());
    CHECK(payments_autofill_client().GetBnplStrategy());
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnablePayNowPayLaterTabs)) {
      std::vector<BnplIssuerContext> issuer_contexts =
          GetSortedBnplIssuerContext(browser_autofill_manager_->client(),
                                     /*checkout_amount=*/std::nullopt,
                                     result.error());
      ReplaceLoadingThrobberWithIssuerSuggestions(issuer_contexts);
    } else {
      using enum BnplStrategy::BeforeSwitchingViewAction;
      switch (payments_autofill_client()
                  .GetBnplStrategy()
                  ->GetBeforeViewSwitchAction()) {
        // This case is for platforms (i.e. Android) that will flip to the
        // error screen within the same view, so no need to remove the current
        // view.
        case kDoNothing:
          break;
        case kCloseCurrentUi:
          HideSuggestionsOrRemoveSelectBnplIssuerOrProgressUi();
          break;
      }

      switch (result.error()) {
        case AiAmountExtractionResult::Error::kFailureToGenerateApc:
        case AiAmountExtractionResult::Error::kMissingServerResponse:
        case AiAmountExtractionResult::Error::kNegativeAmount:
        case AiAmountExtractionResult::Error::kAmountMissing:
        case AiAmountExtractionResult::Error::kMissingCurrency:
        case AiAmountExtractionResult::Error::kTimeout:
          payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
              AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                  /*is_permanent_error=*/false));
          break;
        case AiAmountExtractionResult::Error::kUnsupportedCurrency:
          payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
              AutofillErrorDialogContext::WithBnplUnsupportedCurrencyError());
          break;
      }
      Reset();
    }

    return;
  }

  const std::pair<int64_t, std::string>& amount_and_currency = result.value();
  ongoing_flow_state_->final_checkout_amount = amount_and_currency.first;

  if (IssuerAcceptedAndCheckoutAmountWithinRange()) {
    // If the accepted issuer is eligible, continue the BNPL flow with this
    // issuer.
    OnIssuerAcceptedAndCheckoutAmountAvailable();
  } else {
    std::vector<BnplIssuerContext> issuer_contexts =
        GetSortedBnplIssuerContext(browser_autofill_manager_->client(),
                                   ongoing_flow_state_->final_checkout_amount);
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnablePayNowPayLaterTabs)) {
      ReplaceLoadingThrobberWithIssuerSuggestions(issuer_contexts);
    } else {
      bool is_amount_supported_by_any_issuer =
          IsExtractedAmountSupportedByAnyBnplIssuer(
              payments_autofill_client()
                  .GetPaymentsDataManager()
                  .GetBnplIssuers(),
              ongoing_flow_state_->final_checkout_amount);
      // If the accepted issuer is not eligible, update UI.
      CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
          .UpdateBnplIssuerUi(
              issuer_contexts, ongoing_flow_state_->final_checkout_amount,
              is_amount_supported_by_any_issuer,
              ongoing_flow_state_->app_locale,
              base::BindOnce(&BnplManager::OnIssuerAccepted,
                             weak_factory_.GetWeakPtr()),
              base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));
    }
  }
}

bool BnplManager::AcceptTosActionRequired() const {
  return ongoing_flow_state_->issuer->payment_instrument().has_value() &&
         ongoing_flow_state_->issuer->payment_instrument()
             ->action_required()
             .contains(PaymentInstrument::ActionRequired::kAcceptTos);
}

const std::vector<Suggestion>& BnplManager::GetCachedSuggestions() const {
  return cached_suggestions_;
}

std::vector<Suggestion> BnplManager::GetBnplSuggestions(
    bool is_card_number_field_empty) {
  is_card_number_field_empty_ = is_card_number_field_empty;

  // Both `cached_bnpl_suggestions` and `enforced_order` will always be
  // populated if `cached_suggestions_` is non-empty (i.e. if the autofill popup
  // is already open). `cached_bnpl_suggestions` will be used if
  // `is_card_number_field_empty` is true, otherwise `enforced_order` will be
  // used to generate new disabled BNPL suggestions while keeping the same
  // issuer order.
  std::vector<Suggestion> cached_bnpl_suggestions;
  std::vector<BnplIssuer> enforced_order;
  cached_bnpl_suggestions.reserve(GetCachedSuggestions().size());
  enforced_order.reserve(GetCachedSuggestions().size());
  for (const Suggestion& s : GetCachedSuggestions()) {
    if (s.type == SuggestionType::kBnplEntry) {
      if (const auto* payload =
              std::get_if<Suggestion::BnplIssuer>(&s.payload)) {
        enforced_order.push_back(payload->value());
        cached_bnpl_suggestions.push_back(s);
      }
    } else if (s.type == SuggestionType::kLoadingThrobber) {
      cached_bnpl_suggestions.push_back(s);
    }
  }

  if (!is_card_number_field_empty) {
    // Cancel any ongoing requests, such as amount extraction, in case the
    // user started a flow and then populated the card number field during
    // the flow.
    CancelOngoingRequests();
  }

  std::vector<Suggestion> suggestions;
  if (is_card_number_field_empty && !cached_bnpl_suggestions.empty()) {
    // Prefer cached suggestions if available. This should occur only if the
    // field was interacted with again while the autofill suggestions popup
    // is already open.
    suggestions.append_range(cached_bnpl_suggestions);
  } else {
    // Generate fresh BNPL suggestions. If we are already showing issuer
    // suggestions, i.e. `enforced_order` is non-empty, ensure we keep the same
    // order to avoid reshuffling the issuers.
    const PaymentsDataManager& payments_data_manager =
        browser_autofill_manager_->client()
            .GetPersonalDataManager()
            .payments_data_manager();
    if (is_card_number_field_empty &&
        payments::ShouldStartPayLaterWithLoadingSpinner(
            payments_data_manager)) {
      suggestions.push_back(autofill::GetLoadingSuggestionForPayLaterTab(
          payments_data_manager.GetBnplIssuers().size()));
    } else {
      suggestions.append_range(autofill::GetSuggestionsForBnpl(
          payments::GetSortedBnplIssuerContext(
              browser_autofill_manager_->client(),
              /*checkout_amount=*/std::nullopt,
              /*amount_extraction_error=*/std::nullopt,
              std::move(enforced_order)),
          browser_autofill_manager_->client().GetAppLocale(),
          is_card_number_field_empty));
    }
  }
  return suggestions;
}

void BnplManager::OnSuggestionsHidden(AutofillManager& manager,
                                      SuggestionHidingReason reason) {
  if (reason != SuggestionHidingReason::kHiddenByCaller &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    Reset();
  }
}

bool BnplManager::HasSeenAmountExtractionAiTerms() const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs) &&
      user_has_seen_bnpl_ai_terms_before_.has_value()) {
    return user_has_seen_bnpl_ai_terms_before_.value();
  }
  return payments_autofill_client()
      .GetPaymentsDataManager()
      .IsAutofillAmountExtractionAiTermsSeenPrefEnabled();
}

void BnplManager::FetchVcnDetails(GURL url) {
  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  request_details.billing_customer_number =
      ongoing_flow_state_->billing_customer_number;
  request_details.instrument_id = base::NumberToString(
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id());
  request_details.risk_data = ongoing_flow_state_->risk_data;
  request_details.context_token = ongoing_flow_state_->context_token;
  request_details.redirect_url = std::move(url);
  request_details.issuer_id = autofill::ConvertToBnplIssuerIdString(
      ongoing_flow_state_->issuer->issuer_id());

  CHECK_DEREF(payments_autofill_client().GetBnplUiDelegate())
      .ShowProgressUi(AutofillProgressUiType::kBnplFetchVcnProgressUi,
                      /*cancel_callback=*/base::BindOnce(
                          [](base::WeakPtr<BnplManager> manager) {
                            if (manager) {
                              // Note: Does not call
                              // `BnplUiDelegate::CloseProgressUi()` as this
                              // is expected to be handled by UI code.
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

void BnplManager::CancelOngoingRequests() {
  payments_autofill_client().GetPaymentsNetworkInterface()->CancelRequest();
  browser_autofill_manager_->GetAmountExtractionManager().Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void BnplManager::Reset() {
  CancelOngoingRequests();
  autofill_suggestion_trigger_source_.reset();
  update_suggestions_callback_.Reset();
  user_has_seen_bnpl_ai_terms_before_.reset();
  ongoing_flow_state_.reset();
  cached_suggestions_.clear();
  is_card_number_field_empty_ = false;
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
    credit_card.set_server_id(base::NumberToString(
        ongoing_flow_state_->issuer->payment_instrument()->instrument_id()));
    std::move(ongoing_flow_state_->on_bnpl_vcn_fetched_callback)
        .Run(credit_card);
  } else {
    payments_autofill_client().GetBnplUiDelegate()->ShowAutofillErrorUi(
        AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
            /*is_permanent_error=*/ShouldShowPermanentErrorDialog(result)));
  }
  Reset();
}

bool BnplManager::IssuerAcceptedAndCheckoutAmountWithinRange() {
  // Check eligibility if an issuer was accepted.
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
    if (!has_logged_ai_amount_extracted_in_issuer_range_) {
      autofill_metrics::LogAiAmountExtractedInIssuerRange(
          /*is_within_range=*/false, ongoing_flow_state_->issuer->issuer_id(),
          browser_autofill_manager_->driver().GetPageUkmSourceId());
      has_logged_ai_amount_extracted_in_issuer_range_ = true;
    }
    return false;
  }

  if (!has_logged_ai_amount_extracted_in_issuer_range_) {
    autofill_metrics::LogAiAmountExtractedInIssuerRange(
        /*is_within_range=*/true, ongoing_flow_state_->issuer->issuer_id(),
        browser_autofill_manager_->driver().GetPageUkmSourceId());
    has_logged_ai_amount_extracted_in_issuer_range_ = true;
  }
  return true;
}

void BnplManager::OnIssuerAcceptedAndCheckoutAmountAvailable() {
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
  request_details.instrument_id = base::NumberToString(
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id());
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
      HideSuggestionsOrRemoveSelectBnplIssuerOrProgressUi();
      break;
  }

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    ongoing_flow_state_->context_token = std::move(context_token);

    CHECK(!legal_message.empty());
    payments::BnplTosModel bnpl_tos_model;
    bnpl_tos_model.legal_message_lines = std::move(legal_message);
    bnpl_tos_model.issuer = ongoing_flow_state_->issuer.value();

    payments_autofill_client().GetBnplUiDelegate()->ShowBnplTosUi(
        std::move(bnpl_tos_model),
        base::BindOnce(&BnplManager::OnTosDialogAccepted,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&BnplManager::Reset, weak_factory_.GetWeakPtr()));
    if (ongoing_flow_state_) {
      ongoing_flow_state_->tos_ui_was_shown = true;
    }
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
  request_details.instrument_id = base::NumberToString(
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id());
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
    if (ongoing_flow_state_->tos_ui_was_shown) {
      payments_autofill_client()
          .GetBnplUiDelegate()
          ->RemoveBnplTosOrProgressUi();
    } else {
      // If the ToS UI wasn't shown during this flow, then the autofill
      // suggestions UI, issuer selection UI, or progress UI must be showing, so
      // remove it.
      HideSuggestionsOrRemoveSelectBnplIssuerOrProgressUi();
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
  int64_t instrument_id_int = 0;

  if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess &&
      base::StringToInt64(instrument_id, &instrument_id_int)) {
    ongoing_flow_state_->issuer->set_payment_instrument(PaymentInstrument(
        /*instrument_id=*/instrument_id_int, /*nickname=*/u"",
        /*display_icon_url=*/GURL::EmptyGURL(),
        /*supported_rails=*/
        DenseSet<PaymentInstrument::PaymentRail>(
            {PaymentInstrument::PaymentRail::kCardNumber})));

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
  request_details.instrument_id = base::NumberToString(
      ongoing_flow_state_->issuer->payment_instrument()->instrument_id());
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

void BnplManager::ReplaceLoadingThrobberWithIssuerSuggestions(
    const std::vector<payments::BnplIssuerContext>& issuer_contexts) {
  CHECK(!cached_suggestions_.empty());
  std::vector<Suggestion> new_suggestions = cached_suggestions_;

  // If there is no loading suggestion, then no need to update the current
  // suggestion list.
  auto throbber_it =
      std::find_if(new_suggestions.begin(), new_suggestions.end(),
                   [](const Suggestion& suggestion) {
                     return suggestion.type == SuggestionType::kLoadingThrobber;
                   });
  if (throbber_it == new_suggestions.end()) {
    return;
  }

  std::vector<Suggestion> bnpl_suggestions = GetSuggestionsForBnpl(
      issuer_contexts, browser_autofill_manager_->client().GetAppLocale(),
      /*is_card_number_field_empty=*/true);

  // Replace the loading throbber suggestion with the BNPL suggestions. This
  // ensures that suggestions such as footers are kept after the newly added
  // suggestions.
  throbber_it = new_suggestions.erase(throbber_it);
  new_suggestions.insert(throbber_it,
                         std::make_move_iterator(bnpl_suggestions.begin()),
                         std::make_move_iterator(bnpl_suggestions.end()));

  UpdateAndCacheSuggestions(std::move(new_suggestions));
}

void BnplManager::ReplaceIssuerSuggestionsWithLoadingThrobber() {
  // This function is only called after the Pay Later tab has been shown. At
  // this moment, there has to be suggestions showing.
  CHECK(!cached_suggestions_.empty());

  auto type_is_bnpl_entry = [](const Suggestion& s) {
    return s.type == SuggestionType::kBnplEntry;
  };
  auto type_is_not_bnpl_entry = [](const Suggestion& s) {
    return s.type != SuggestionType::kBnplEntry;
  };

  // Find the start position of BNPL suggestions.
  auto bnpl_suggestions_start =
      std::find_if(cached_suggestions_.begin(), cached_suggestions_.end(),
                   type_is_bnpl_entry);

  // If there is no BNPL suggestions in the suggestion list, there has to be a
  // loading throbber suggestion. Therefore, no need to update the suggestion
  // list.
  if (bnpl_suggestions_start == cached_suggestions_.end()) {
    CHECK(std::find_if(cached_suggestions_.begin(), cached_suggestions_.end(),
                       [](const Suggestion& suggestion) {
                         return suggestion.type ==
                                SuggestionType::kLoadingThrobber;
                       }) != cached_suggestions_.end());
    return;
  }

  // Find the end position of BNPL suggestions.
  auto bnpl_suggestions_end =
      std::find_if(bnpl_suggestions_start, cached_suggestions_.end(),
                   type_is_not_bnpl_entry);

  // When there are pay later BNPL suggestions, there must be footer
  // suggestions with different suggestion type after the BNPL entries.
  CHECK(bnpl_suggestions_end != cached_suggestions_.end());
  // BNPL suggestions are inserted together into the suggestion list and there
  // should be no other BNPL suggestions after `bnpl_suggestions_end`.
  CHECK(std::ranges::none_of(bnpl_suggestions_end, cached_suggestions_.end(),
                             type_is_bnpl_entry));

  int bnpl_suggestion_count =
      std::distance(bnpl_suggestions_start, bnpl_suggestions_end);
  std::vector<Suggestion> updated_suggestions;
  // All BNPL suggestions will be replaced by a single loading suggestion.
  updated_suggestions.reserve(cached_suggestions_.size() -
                              bnpl_suggestion_count + 1);

  // Copy suggestions before BNPL entries.
  updated_suggestions.insert(updated_suggestions.end(),
                             cached_suggestions_.begin(),
                             bnpl_suggestions_start);
  // Insert the loading suggestion based on number of BNPL suggestions.
  updated_suggestions.push_back(
      GetLoadingSuggestionForPayLaterTab(bnpl_suggestion_count));
  // Copy the remaining suggestions.
  updated_suggestions.insert(updated_suggestions.end(), bnpl_suggestions_end,
                             cached_suggestions_.end());

  UpdateAndCacheSuggestions(std::move(updated_suggestions));
}

void BnplManager::HideSuggestionsOrRemoveSelectBnplIssuerOrProgressUi() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    browser_autofill_manager_->client().HideAutofillSuggestions(
        SuggestionHidingReason::kHiddenByCaller);
  } else {
    payments_autofill_client()
        .GetBnplUiDelegate()
        ->RemoveSelectBnplIssuerOrProgressUi();
  }
}

void BnplManager::UpdateAndCacheSuggestions(
    std::vector<Suggestion> updated_suggestions) {
  cached_suggestions_ = updated_suggestions;
  CHECK(autofill_suggestion_trigger_source_.has_value());
  update_suggestions_callback_.Run(std::move(updated_suggestions),
                                   autofill_suggestion_trigger_source_.value());
}

}  // namespace autofill::payments
