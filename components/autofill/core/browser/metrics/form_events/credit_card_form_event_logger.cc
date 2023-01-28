// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

#include <string>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

CreditCardFormEventLogger::CreditCardFormEventLogger(
    bool is_in_any_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    PersonalDataManager* personal_data_manager,
    AutofillClient* client)
    : FormEventLoggerBase("CreditCard",
                          is_in_any_main_frame,
                          form_interactions_ukm_logger,
                          client),
      current_authentication_flow_(UnmaskAuthFlowType::kNone),
      personal_data_manager_(personal_data_manager),
      client_(client) {}

CreditCardFormEventLogger::~CreditCardFormEventLogger() = default;

void CreditCardFormEventLogger::OnDidFetchSuggestion(
    const std::vector<Suggestion>& suggestions,
    bool with_offer,
    const autofill_metrics::CardMetadataLoggingContext&
        metadata_logging_context) {
  has_eligible_offer_ = with_offer;
  metadata_logging_context_ = metadata_logging_context;
  suggestions_.clear();
  for (const auto& suggestion : suggestions)
    suggestions_.emplace_back(suggestion);
}

void CreditCardFormEventLogger::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp,
    AutofillSyncSigninState sync_state,
    bool off_the_record) {
  if (DoSuggestionsIncludeVirtualCard())
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, form);

  // Also perform the logging actions from the base class:
  FormEventLoggerBase::OnDidShowSuggestions(form, field, form_parsed_timestamp,
                                            sync_state, off_the_record);

  suggestion_shown_timestamp_ = AutofillTickClock::NowTicks();
}

void CreditCardFormEventLogger::OnDidSelectCardSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;

  card_selected_has_offer_ = false;
  if (has_eligible_offer_) {
    card_selected_has_offer_ = DoesCardHaveOffer(credit_card);
    base::UmaHistogramBoolean("Autofill.Offer.SelectedCardHasOffer",
                              card_selected_has_offer_);
  }

  latest_selected_card_was_virtual_card_ = false;
  switch (credit_card.record_type()) {
    case CreditCard::LOCAL_CARD:
    case CreditCard::FULL_SERVER_CARD:
      // No need to log selections for local/full-server cards -- a selection is
      // always followed by a form fill, which is logged separately.
      break;
    case CreditCard::MASKED_SERVER_CARD:
      Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, form);
      if (!has_logged_masked_server_card_suggestion_selected_) {
        has_logged_masked_server_card_suggestion_selected_ = true;
        Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, form);
      }
      break;
    case CreditCard::VIRTUAL_CARD:
      latest_selected_card_was_virtual_card_ = true;
      Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, form);
      if (!has_logged_virtual_card_suggestion_selected_) {
        has_logged_virtual_card_suggestion_selected_ = true;
        Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, form);
      }
      break;
  }

  autofill_metrics::LogAcceptanceLatency(
      AutofillTickClock::NowTicks() - suggestion_shown_timestamp_,
      metadata_logging_context_, credit_card);
}

void CreditCardFormEventLogger::OnDidFillSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    const AutofillField& field,
    const base::flat_set<FieldGlobalId>& newly_filled_fields,
    const base::flat_set<FieldGlobalId>& safe_fields,
    AutofillSyncSigninState sync_state) {
  CreditCard::RecordType record_type = credit_card.record_type();
  sync_state_ = sync_state;
  ukm::builders::Autofill_CreditCardFill builder =
      form_interactions_ukm_logger_->CreateCreditCardFillBuilder();
  builder.SetFormSignature(HashFormSignature(form.form_signature()));

  form_interactions_ukm_logger_->LogDidFillSuggestion(
      record_type,
      /*is_for_credit_card=*/true, form, field);

  AutofillMetrics::LogCreditCardSeamlessnessAtFillTime(
      {.event_logger = raw_ref(*this),
       .form = raw_ref(form),
       .field = raw_ref(field),
       .newly_filled_fields = raw_ref(newly_filled_fields),
       .safe_fields = raw_ref(safe_fields),
       .builder = raw_ref(builder)});

  switch (record_type) {
    case CreditCard::LOCAL_CARD:
      Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
      break;
    case CreditCard::MASKED_SERVER_CARD:
      Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, form);
      break;
    case CreditCard::FULL_SERVER_CARD:
      Log(FORM_EVENT_SERVER_SUGGESTION_FILLED, form);
      break;
    case CreditCard::VIRTUAL_CARD:
      Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, form);
      break;
  }

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        record_type == CreditCard::MASKED_SERVER_CARD ||
        record_type == CreditCard::FULL_SERVER_CARD ||
        record_type == CreditCard::VIRTUAL_CARD;
    logged_suggestion_filled_was_masked_server_card_ =
        record_type == CreditCard::MASKED_SERVER_CARD;
    logged_suggestion_filled_was_virtual_card_ =
        record_type == CreditCard::VIRTUAL_CARD;
    switch (record_type) {
      case CreditCard::LOCAL_CARD:
        // Check if the local card is a duplicate of an existing server card
        // and log an additional metric if so.
        if (IsLocalDuplicateOfServerCard(credit_card)) {
          Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              form);
        }
        Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
        break;
      case CreditCard::MASKED_SERVER_CARD:
        Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, form);
        break;
      case CreditCard::FULL_SERVER_CARD:
        Log(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, form);
        break;
      case CreditCard::VIRTUAL_CARD:
        Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, form);
        break;
    }
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledCreditCardSuggestion"));

  form_interactions_ukm_logger_->Record(std::move(builder));

  ++form_interaction_counts_.autofill_fills;
  UpdateFlowId();
}

void CreditCardFormEventLogger::LogCardUnmaskAuthenticationPromptShown(
    UnmaskAuthFlowType flow) {
  RecordCardUnmaskFlowEvent(flow, UnmaskAuthFlowEvent::kPromptShown);
}

void CreditCardFormEventLogger::LogCardUnmaskAuthenticationPromptCompleted(
    UnmaskAuthFlowType flow) {
  RecordCardUnmaskFlowEvent(flow, UnmaskAuthFlowEvent::kPromptCompleted);

  // Keeping track of authentication type in order to split form-submission
  // metrics.
  current_authentication_flow_ = flow;
}

void CreditCardFormEventLogger::RecordPollSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_PolledCreditCardSuggestions"));
}

void CreditCardFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedCreditCardForm"));
}

void CreditCardFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedCreditCardSuggestions"));
}

void CreditCardFormEventLogger::LogWillSubmitForm(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_virtual_card_) {
    Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, form);
  }
}

void CreditCardFormEventLogger::LogFormSubmitted(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, form);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, form);

    // Log BetterAuth.FlowEvents.
    RecordCardUnmaskFlowEvent(current_authentication_flow_,
                              UnmaskAuthFlowEvent::kFormSubmitted);
  } else if (logged_suggestion_filled_was_virtual_card_) {
    Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, form);

    // Log BetterAuth.FlowEvents.
    RecordCardUnmaskFlowEvent(current_authentication_flow_,
                              UnmaskAuthFlowEvent::kFormSubmitted);
    autofill_metrics::LogServerCardUnmaskFormSubmission(
        AutofillClient::PaymentsRpcCardType::kVirtualCard);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }

  if (has_logged_suggestion_filled_ && has_eligible_offer_) {
    base::UmaHistogramBoolean("Autofill.Offer.SubmittedCardHasOffer",
                              card_selected_has_offer_);
  }
}

void CreditCardFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  form_interactions_ukm_logger_->LogInteractedWithForm(
      /*is_for_credit_card=*/true, local_record_type_count_,
      server_record_type_count_, form_signature);
}

void CreditCardFormEventLogger::OnSuggestionsShownOnce(
    const FormStructure& form) {
  if (DoSuggestionsIncludeVirtualCard())
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, form);

  base::UmaHistogramBoolean("Autofill.Offer.SuggestedCardsHaveOffer",
                            has_eligible_offer_);
}

void CreditCardFormEventLogger::OnSuggestionsShownSubmittedOnce(
    const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    const CreditCard& credit_card =
        client_->GetFormDataImporter()->ExtractCreditCardFromForm(form).card;
    Log(GetCardNumberStatusFormEvent(credit_card), form);
  }
}

void CreditCardFormEventLogger::OnLog(const std::string& name,
                                      FormEvent event,
                                      const FormStructure& form) const {
  // Log a different histogram for credit card forms with credit card offers
  // available so that selection rate with offers and rewards can be compared on
  // their own.
  if (has_eligible_offer_) {
    base::UmaHistogramEnumeration(name + ".WithOffer", event, NUM_FORM_EVENTS);
  }
}

bool CreditCardFormEventLogger::IsLocalDuplicateOfServerCard(
    const CreditCard& credit_card) {
  // Get the list of all the server credit cards for the user and see if any
  // card in the list matches/isDuplicateOf the local card.
  return base::ranges::any_of(
      personal_data_manager_->GetServerCreditCards(),
      [&credit_card](CreditCard* card_from_list) {
        return credit_card.IsLocalDuplicateOfServerCard(*card_from_list);
      });
}

FormEvent CreditCardFormEventLogger::GetCardNumberStatusFormEvent(
    const CreditCard& credit_card) {
  const std::u16string number = credit_card.number();
  FormEvent form_event =
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD;

  if (number.empty()) {
    form_event = FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD;
  } else if (!HasCorrectLength(number)) {
    form_event =
        FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD;
  } else if (!PassesLuhnCheck(number)) {
    form_event =
        FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD;
  } else if (personal_data_manager_->IsKnownCard(credit_card)) {
    form_event = FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD;
  }

  return form_event;
}

void CreditCardFormEventLogger::RecordCardUnmaskFlowEvent(
    UnmaskAuthFlowType flow,
    UnmaskAuthFlowEvent event) {
  std::string flow_type_suffix;
  switch (flow) {
    case UnmaskAuthFlowType::kCvc:
      flow_type_suffix = ".Cvc";
      break;
    case UnmaskAuthFlowType::kFido:
      flow_type_suffix = ".Fido";
      break;
    case UnmaskAuthFlowType::kCvcThenFido:
      flow_type_suffix = ".CvcThenFido";
      break;
    case UnmaskAuthFlowType::kCvcFallbackFromFido:
      flow_type_suffix = ".CvcFallbackFromFido";
      break;
    case UnmaskAuthFlowType::kOtp:
      flow_type_suffix = ".Otp";
      break;
    case UnmaskAuthFlowType::kOtpFallbackFromFido:
      flow_type_suffix = ".OtpFallbackFromFido";
      break;
    case UnmaskAuthFlowType::kNone:
      // TODO(crbug.com/1300959): Fix Autofill.BetterAuth logging.
      return;
  }
  std::string card_type_suffix =
      latest_selected_card_was_virtual_card_ ? ".VirtualCard" : ".ServerCard";

  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.FlowEvents" + flow_type_suffix, event);
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.FlowEvents" + flow_type_suffix + card_type_suffix,
      event);
}

bool CreditCardFormEventLogger::DoesCardHaveOffer(
    const CreditCard& credit_card) {
  auto* offer_manager = client_->GetAutofillOfferManager();
  if (!offer_manager)
    return false;

  auto card_linked_offer_map = offer_manager->GetCardLinkedOffersMap(
      client_->GetLastCommittedPrimaryMainFrameURL());
  return base::Contains(card_linked_offer_map, credit_card.guid());
}

bool CreditCardFormEventLogger::DoSuggestionsIncludeVirtualCard() {
  auto is_virtual_card = [](const Suggestion& suggestion) {
    return suggestion.frontend_id == POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY;
  };
  return base::ranges::any_of(suggestions_, is_virtual_card);
}

}  // namespace autofill
