// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_standalone_cvc_suggestion_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

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
    bool with_cvc,
    bool is_virtual_card_standalone_cvc_field,
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context) {
  has_eligible_offer_ = with_offer;
  suggestion_contains_card_with_cvc_ = with_cvc;
  is_virtual_card_standalone_cvc_field_ = is_virtual_card_standalone_cvc_field;
  metadata_logging_context_ = std::move(metadata_logging_context);
  suggestions_.clear();
  for (const auto& suggestion : suggestions)
    suggestions_.emplace_back(suggestion);
}

void CreditCardFormEventLogger::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record) {
  if (DoSuggestionsIncludeVirtualCard())
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, form);

  // Also perform the logging actions from the base class:
  FormEventLoggerBase::OnDidShowSuggestions(form, field, form_parsed_timestamp,
                                            off_the_record);

  suggestion_shown_timestamp_ = base::TimeTicks::Now();

  // Log if standalone CVC suggestions were shown for virtual cards.
  if (is_virtual_card_standalone_cvc_field_) {
    LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
        VirtualCardStandaloneCvcSuggestionFormEvent::
            kStandaloneCvcSuggestionShown);
    if (!has_logged_suggestion_for_virtual_card_standalone_cvc_shown_) {
      LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
          VirtualCardStandaloneCvcSuggestionFormEvent::
              kStandaloneCvcSuggestionShownOnce);
    }
    has_logged_suggestion_for_virtual_card_standalone_cvc_shown_ = true;
  }

  // Log if any of the card suggestions had cvc saved.
  if (suggestion_contains_card_with_cvc_) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN, form);
    if (!has_logged_suggestion_for_card_with_cvc_shown_) {
      Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE, form);
    }
    has_logged_suggestion_for_card_with_cvc_shown_ = true;
  }

  // Log if any of the suggestions had metadata.
  Log(!metadata_logging_context_.instruments_with_metadata_available.empty()
          ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN
          : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN,
      form);
  if (!has_logged_suggestion_with_metadata_shown_) {
    Log(!metadata_logging_context_.instruments_with_metadata_available.empty()
            ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN_ONCE
            : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN_ONCE,
        form);
  }
  // Log issuer-specific metrics on whether card suggestions shown had metadata.
  LogCardWithMetadataFormEventMetric(
      autofill_metrics::CardMetadataLoggingEvent::kShown,
      metadata_logging_context_,
      HasBeenLogged(has_logged_suggestion_with_metadata_shown_));
  has_logged_suggestion_with_metadata_shown_ = true;

  // Log if any of the suggestions had benefit available.
  if (!has_logged_suggestion_shown_for_benefits_) {
    if (metadata_logging_context_.DidShowCardWithBenefitAvailable()) {
      Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE,
          form);
    }
    LogCardWithBenefitFormEventMetric(
        autofill_metrics::CardMetadataLoggingEvent::kShown,
        metadata_logging_context_);
    has_logged_suggestion_shown_for_benefits_ = true;
  }
  if (metadata_logging_context_.DidShowCardWithBenefitAvailable()) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_BENEFIT_AVAILABLE_SHOWN, form);
  }
}

void CreditCardFormEventLogger::LogDeprecatedCreditCardSelectedMetric(
    const CreditCard& credit_card,
    const FormStructure& form,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
  signin_state_for_metrics_ = signin_state_for_metrics;

  switch (credit_card.record_type()) {
    case CreditCard::RecordType::kLocalCard:
    case CreditCard::RecordType::kFullServerCard:
    case CreditCard::RecordType::kVirtualCard:
      // No need to log selections for these types; crbug/1513307 only focused
      // on masked server cards.
      break;
    case CreditCard::RecordType::kMaskedServerCard:
      Log(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, form);
      if (!has_logged_legacy_masked_server_card_suggestion_selected_) {
        has_logged_legacy_masked_server_card_suggestion_selected_ = true;
        Log(DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
            form);
      }
      break;
  }
}

void CreditCardFormEventLogger::OnDidSelectCardSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  metadata_logging_context_.SetSelectedCardInfo(credit_card);

  card_selected_has_offer_ = false;
  if (has_eligible_offer_) {
    card_selected_has_offer_ = DoesCardHaveOffer(credit_card);
    base::UmaHistogramBoolean("Autofill.Offer.SelectedCardHasOffer",
                              card_selected_has_offer_);
  }

  latest_selected_card_was_virtual_card_ = false;
  switch (credit_card.record_type()) {
    case CreditCard::RecordType::kLocalCard:
      Log(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED, form);
      if (!has_logged_local_card_suggestion_selected_) {
        has_logged_local_card_suggestion_selected_ = true;
        Log(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE, form);
      }
      break;
    case CreditCard::RecordType::kMaskedServerCard:
      Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, form);

      if (!has_logged_masked_server_card_suggestion_selected_) {
        has_logged_masked_server_card_suggestion_selected_ = true;
        Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, form);
        if (personal_data_manager_->payments_data_manager()
                .IsCardPresentAsBothLocalAndServerCards(credit_card)) {
          Log(FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              form);
        }

        // Log masked server card selected once events for benefits.
        if (metadata_logging_context_.SelectedCardHasBenefitAvailable()) {
          Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SELECTED_ONCE,
              form);
        }
        // Log when a masked server card was selected after benefits were shown.
        if (metadata_logging_context_.DidShowCardWithBenefitAvailable()) {
          Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SELECTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE,
              form);
          LogCardWithBenefitFormEventMetric(
              autofill_metrics::CardMetadataLoggingEvent::kSelected,
              metadata_logging_context_);
        }
      }

      // Log masked server card selected events for benefits.
      if (metadata_logging_context_.SelectedCardHasBenefitAvailable()) {
        Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SELECTED,
            form);
      }

      break;
    case CreditCard::RecordType::kVirtualCard:
      latest_selected_card_was_virtual_card_ = true;
      Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, form);
      if (!has_logged_virtual_card_suggestion_selected_) {
        has_logged_virtual_card_suggestion_selected_ = true;
        Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, form);
      }
      break;
    case CreditCard::RecordType::kFullServerCard:
      // Full server cards are a temporary cached state that do not exist as
      // suggestions, and so should never be passed to this method. Even when a
      // card is being re-filled in a page, the suggestion itself is a
      // kMaskedServerCard and its corresponding kFullServerCard is looked up in
      // a cache during the fill.
      NOTREACHED();
  }

  autofill_metrics::LogAcceptanceLatency(
      base::TimeTicks::Now() - suggestion_shown_timestamp_,
      metadata_logging_context_, credit_card);

  // Log if a CVC suggestion was selected for a virtual card.
  if (is_virtual_card_standalone_cvc_field_) {
    LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
        VirtualCardStandaloneCvcSuggestionFormEvent::
            kStandaloneCvcSuggestionSelected);
    if (!has_logged_suggestion_for_virtual_card_standalone_cvc_selected_) {
      LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
          VirtualCardStandaloneCvcSuggestionFormEvent::
              kStandaloneCvcSuggestionSelectedOnce);
    }
    has_logged_suggestion_for_virtual_card_standalone_cvc_selected_ = true;
  }

  // Log if the selected card suggestion had cvc saved.
  if (!credit_card.cvc().empty()) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED, form);
    if (!has_logged_suggestion_for_card_with_cvc_selected_) {
      Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE, form);
    }
    has_logged_suggestion_for_card_with_cvc_selected_ = true;
  }

  // Log if the selected suggestion had metadata.
  Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
          ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED
          : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED,
      form);
  if (!has_logged_suggestion_with_metadata_selected_) {
    Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
            ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED_ONCE
            : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED_ONCE,
        form);

    if (suggestions_.size() > 1) {
      CHECK(personal_data_manager_);
      // Keeps track of which issuers and networks with metadata were not
      // selected. Can be none if there was only one card suggestion displayed
      // and that card was selected.
      for (const Suggestion& suggestion : suggestions_) {
        // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
        const CreditCard* suggested_credit_card =
            personal_data_manager_->payments_data_manager().GetCreditCardByGUID(
                suggestion.GetBackendId<Suggestion::Guid>().value());
        if (!suggested_credit_card) {
          // Ignore non credit card suggestions in the popup like separators,
          // manage payment methods, etc.
          continue;
        }
        if (credit_card.issuer_id() != suggested_credit_card->issuer_id() &&
            (suggested_credit_card->HasRichCardArtImageFromMetadata() ||
             !suggested_credit_card->product_description().empty())) {
          metadata_logging_context_.not_selected_issuer_ids_and_networks.insert(
              suggested_credit_card->issuer_id());
        }
        // Skip American Express and Discover as they are an issuer and
        // network. They are already covered in the `issuer_id` check above.
        if (credit_card.network() != kAmericanExpressCard &&
            credit_card.network() != kDiscoverCard &&
            credit_card.network() != suggested_credit_card->network()) {
          metadata_logging_context_.not_selected_issuer_ids_and_networks.insert(
              suggested_credit_card->network());
        }
      }
    }
  }
  LogCardWithMetadataFormEventMetric(
      autofill_metrics::CardMetadataLoggingEvent::kSelected,
      metadata_logging_context_,
      HasBeenLogged(has_logged_suggestion_with_metadata_selected_));
  has_logged_suggestion_with_metadata_selected_ = true;
}

void CreditCardFormEventLogger::OnDidFillFormFillingSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    const AutofillField& field,
    const base::flat_set<FieldGlobalId>& newly_filled_fields,
    const base::flat_set<FieldGlobalId>& safe_fields,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    const AutofillTriggerSource trigger_source) {
  CreditCard::RecordType record_type = credit_card.record_type();
  signin_state_for_metrics_ = signin_state_for_metrics;
  ukm::builders::Autofill_CreditCardFill builder =
      form_interactions_ukm_logger_->CreateCreditCardFillBuilder();
  builder.SetFormSignature(HashFormSignature(form.form_signature()));

  form_interactions_ukm_logger_->LogDidFillSuggestion(form, field, record_type);

  AutofillMetrics::LogCreditCardSeamlessnessAtFillTime(
      {.event_logger = raw_ref(*this),
       .form = raw_ref(form),
       .field = raw_ref(field),
       .newly_filled_fields = raw_ref(newly_filled_fields),
       .safe_fields = raw_ref(safe_fields),
       .builder = raw_ref(builder)});

  latest_filled_card_was_masked_server_card_ = false;
  switch (record_type) {
    case CreditCard::RecordType::kLocalCard:
      Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
      break;
    case CreditCard::RecordType::kMaskedServerCard:
      Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, form);
      latest_filled_card_was_masked_server_card_ = true;
      break;
    case CreditCard::RecordType::kVirtualCard:
      Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, form);
      break;
    case CreditCard::RecordType::kFullServerCard:
      // Full server cards are a temporary cached state that do not exist as
      // suggestions, and so should never be passed to this method. Even when a
      // card is being re-filled in a page, the suggestion itself is a
      // kMaskedServerCard and its corresponding kFullServerCard is looked up in
      // a cache later in the fill.
      NOTREACHED();
  }

  // Log if a standalone CVC field was filled with Autofill suggestion for a
  // virtual card.
  if (is_virtual_card_standalone_cvc_field_) {
    LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
        VirtualCardStandaloneCvcSuggestionFormEvent::
            kStandaloneCvcSuggestionFilled);
    if (!has_logged_suggestion_for_virtual_card_standalone_cvc_filled_) {
      LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
          VirtualCardStandaloneCvcSuggestionFormEvent::
              kStandaloneCvcSuggestionFilledOnce);
    }
    has_logged_suggestion_for_virtual_card_standalone_cvc_filled_ = true;
  }

  // Log if the filled card suggestion had cvc saved.
  if (!credit_card.cvc().empty()) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED, form);
    if (!has_logged_suggestion_for_card_with_cvc_filled_) {
      Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE, form);
    }
    has_logged_suggestion_for_card_with_cvc_filled_ = true;
  }

  // Log if the filled suggestion had metadata.
  Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
          ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED
          : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED,
      form);
  // Log issuer-specific metrics on whether a card suggestion with metadata
  // was filled.
  LogCardWithMetadataFormEventMetric(
      autofill_metrics::CardMetadataLoggingEvent::kFilled,
      metadata_logging_context_,
      HasBeenLogged(has_logged_form_filling_suggestion_filled_));

  // Log masked server card filled events for benefits.
  if (latest_filled_card_was_masked_server_card_) {
    if (metadata_logging_context_.SelectedCardHasBenefitAvailable()) {
      Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_FILLED,
          form);
    }

    if (!has_logged_masked_server_card_suggestion_filled_) {
      has_logged_masked_server_card_suggestion_filled_ = true;
      if (metadata_logging_context_.SelectedCardHasBenefitAvailable()) {
        Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_FILLED_ONCE,
            form);
      }
      // Log when a masked server card was filled after benefits were shown.
      if (metadata_logging_context_.DidShowCardWithBenefitAvailable()) {
        Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_FILLED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE,
            form);
        LogCardWithBenefitFormEventMetric(
            autofill_metrics::CardMetadataLoggingEvent::kFilled,
            metadata_logging_context_);
      }
    }
  }

  if (!has_logged_form_filling_suggestion_filled_) {
    has_logged_form_filling_suggestion_filled_ = true;
    logged_suggestion_filled_was_masked_server_card_ =
        record_type == CreditCard::RecordType::kMaskedServerCard;
    logged_suggestion_filled_was_virtual_card_ =
        record_type == CreditCard::RecordType::kVirtualCard;
    switch (record_type) {
      case CreditCard::RecordType::kLocalCard:
        // Check if the local card is a duplicate of an existing server card
        // and log an additional metric if so.
        if (personal_data_manager_->payments_data_manager()
                .IsCardPresentAsBothLocalAndServerCards(credit_card)) {
          Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              form);
        }
        Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
        break;
      case CreditCard::RecordType::kMaskedServerCard:
        Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, form);
        if (personal_data_manager_->payments_data_manager()
                .IsCardPresentAsBothLocalAndServerCards(credit_card)) {
          Log(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              form);
          server_card_with_local_duplicate_filled_ = true;
        }
        break;
      case CreditCard::RecordType::kVirtualCard:
        Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, form);
        break;
      case CreditCard::RecordType::kFullServerCard:
        // Full server cards are a temporary cached state that do not exist as
        // suggestions, and so should never be passed to this method. Even when
        // a card is being re-filled in a page, the suggestion itself is a
        // kMaskedServerCard and its corresponding kFullServerCard is looked up
        // in a cache later in the fill.
        NOTREACHED();
    }
    // Log if filled suggestions had metadata. Logged once per page load.
    Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
            ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED_ONCE
            : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED_ONCE,
        form);
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledCreditCardSuggestion"));

  form_interactions_ukm_logger_->Record(std::move(builder));

  if (trigger_source != AutofillTriggerSource::kFastCheckout) {
    ++form_interaction_counts_.autofill_fills;
  }
  UpdateFlowId();
}

void CreditCardFormEventLogger::OnDidUndoAutofill() {
  has_logged_undo_after_fill_ = true;
  base::RecordAction(base::UserMetricsAction("Autofill_UndoPaymentsAutofill"));
}

void CreditCardFormEventLogger::Log(FormEvent event,
                                    const FormStructure& form) {
  FormEventLoggerBase::Log(event, form);
  const std::string_view data_suffix = [&]() {
    if (server_record_type_count_ == 0 && local_record_type_count_ == 0) {
      return ".WithNoData";
    } else if (server_record_type_count_ > 0 && local_record_type_count_ == 0) {
      return ".WithOnlyServerData";
    } else if (server_record_type_count_ == 0 && local_record_type_count_ > 0) {
      return ".WithOnlyLocalData";
    };
    return ".WithBothServerAndLocalData";
  }();
  for (FormTypeNameForLogging form_type :
       base::FeatureList::IsEnabled(
           features::kAutofillEnableLogFormEventsToAllParsedFormTypes)
           ? parsed_form_types_
           : GetFormTypesForLogging(form)) {
    std::string name = base::StrCat(
        {"Autofill.FormEvents.", FormTypeNameForLoggingToStringView(form_type),
         data_suffix});
    base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);
    base::UmaHistogramEnumeration(
        name + AutofillMetrics::GetMetricsSyncStateSuffix(
                   signin_state_for_metrics_),
        event, NUM_FORM_EVENTS);
  }
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
  if (!has_logged_form_filling_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_virtual_card_) {
    Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, form);
  }

  // Log if a CVC suggestion for a virtual card was filled before form
  // submission.
  if (is_virtual_card_standalone_cvc_field_ &&
      has_logged_suggestion_for_virtual_card_standalone_cvc_filled_) {
    LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
        VirtualCardStandaloneCvcSuggestionFormEvent::
            kStandaloneCvcSuggestionWillSubmitOnce);
  }

  // Log if any card suggestion with cvc saved was filled before form
  // submission.
  if (has_logged_suggestion_for_card_with_cvc_filled_) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_WILL_SUBMIT_ONCE, form);
  }

  if (has_logged_form_filling_suggestion_filled_) {
    // Log issuer-specific metrics on whether a card suggestion with metadata
    // was filled before submission.
    LogCardWithMetadataFormEventMetric(
        autofill_metrics::CardMetadataLoggingEvent::kWillSubmit,
        metadata_logging_context_, HasBeenLogged(false));
    // If a card suggestion was filled before submission, log it for metadata.
    // This event can only be triggered once per page load.
    Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
            ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_WILL_SUBMIT_ONCE
            : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_WILL_SUBMIT_ONCE,
        form);
  }
}

void CreditCardFormEventLogger::LogFormSubmitted(const FormStructure& form) {
  if (!has_logged_form_filling_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, form);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, form);
    if (server_card_with_local_duplicate_filled_) {
      Log(FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
          form);
    }

    // Log BetterAuth.FlowEvents.
    RecordCardUnmaskFlowEvent(current_authentication_flow_,
                              UnmaskAuthFlowEvent::kFormSubmitted);
  } else if (logged_suggestion_filled_was_virtual_card_) {
    Log(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, form);

    // Log BetterAuth.FlowEvents.
    RecordCardUnmaskFlowEvent(current_authentication_flow_,
                              UnmaskAuthFlowEvent::kFormSubmitted);
    autofill_metrics::LogServerCardUnmaskFormSubmission(
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }

  if (has_logged_form_filling_suggestion_filled_ && has_eligible_offer_) {
    base::UmaHistogramBoolean("Autofill.Offer.SubmittedCardHasOffer",
                              card_selected_has_offer_);
  }

  // Log if a CVC suggestion for a virtual card was filled before form
  // submission.
  if (is_virtual_card_standalone_cvc_field_ &&
      has_logged_suggestion_for_virtual_card_standalone_cvc_filled_) {
    LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
        VirtualCardStandaloneCvcSuggestionFormEvent::
            kStandaloneCvcSuggestionSubmittedOnce);
  }

  // Log if any card suggestion with cvc saved was filled before form
  // submission.
  if (has_logged_suggestion_for_card_with_cvc_filled_) {
    Log(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SUBMITTED_ONCE, form);
  }

  if (has_logged_form_filling_suggestion_filled_) {
    // Log issuer-specific metrics on whether a card suggestion with metadata
    // was filled before submission.
    LogCardWithMetadataFormEventMetric(
        autofill_metrics::CardMetadataLoggingEvent::kSubmitted,
        metadata_logging_context_, HasBeenLogged(false));
    // If a card suggestion was filled before submission, log it for metadata.
    // This event can only be triggered once per page load.
    Log(metadata_logging_context_.SelectedCardHasMetadataAvailable()
            ? FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SUBMITTED_ONCE
            : FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SUBMITTED_ONCE,
        form);
  }

  // Log masked server card submitted events for benefits.
  if (latest_filled_card_was_masked_server_card_) {
    if (metadata_logging_context_.SelectedCardHasBenefitAvailable()) {
      Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SUBMITTED_ONCE,
          form);
    }
    // Log when a form is submitted after a suggestion for a card with benefits
    // was shown. The user may have selected a card other than the card with
    // benefits.
    if (metadata_logging_context_.DidShowCardWithBenefitAvailable()) {
      Log(FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SUBMITTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE,
          form);
      LogCardWithBenefitFormEventMetric(
          autofill_metrics::CardMetadataLoggingEvent::kSubmitted,
          metadata_logging_context_);
    }
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
  if (!has_logged_form_filling_suggestion_filled_) {
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

bool CreditCardFormEventLogger::HasLoggedDataToFillAvailable() const {
  return server_record_type_count_ + local_record_type_count_ > 0;
}

DenseSet<FormTypeNameForLogging>
CreditCardFormEventLogger::GetSupportedFormTypeNamesForLogging() const {
  return {FormTypeNameForLogging::kCreditCardForm,
          FormTypeNameForLogging::kStandaloneCvcForm};
}

DenseSet<FormTypeNameForLogging>
CreditCardFormEventLogger::GetFormTypesForLogging(
    const FormStructure& form) const {
  return GetCreditCardFormTypesForLogging(form);
}

FormEvent CreditCardFormEventLogger::GetCardNumberStatusFormEvent(
    const CreditCard& credit_card) {
  const std::u16string number = credit_card.number();
  FormEvent form_event =
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD;

  if (number.empty()) {
    form_event = FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD;
  } else if (!HasCorrectCreditCardNumberLength(number)) {
    form_event =
        FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD;
  } else if (!PassesLuhnCheck(number)) {
    form_event =
        FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD;
  } else if (personal_data_manager_->payments_data_manager().IsKnownCard(
                 credit_card)) {
    form_event = FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD;
  }

  return form_event;
}

void CreditCardFormEventLogger::RecordCardUnmaskFlowEvent(
    UnmaskAuthFlowType flow,
    UnmaskAuthFlowEvent event) {
  std::string_view flow_type_suffix;
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
    case UnmaskAuthFlowType::kThreeDomainSecure:
    case UnmaskAuthFlowType::kThreeDomainSecureConsentAlreadyGiven:
      flow_type_suffix = ".ThreeDomainSecure";
      break;
    case UnmaskAuthFlowType::kNone:
      // TODO(crbug.com/40216473): Fix Autofill.BetterAuth logging.
      return;
  }
  std::string_view card_type_suffix =
      latest_selected_card_was_virtual_card_ ? ".VirtualCard" : ".ServerCard";

  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.BetterAuth.FlowEvents", flow_type_suffix}),
      event);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.BetterAuth.FlowEvents", flow_type_suffix,
                    card_type_suffix}),
      event);
}

bool CreditCardFormEventLogger::DoesCardHaveOffer(
    const CreditCard& credit_card) {
  auto* offer_manager =
      client_->GetPaymentsAutofillClient()->GetAutofillOfferManager();
  if (!offer_manager)
    return false;

  auto card_linked_offer_map = offer_manager->GetCardLinkedOffersMap(
      client_->GetLastCommittedPrimaryMainFrameURL());
  return base::Contains(card_linked_offer_map, credit_card.guid());
}

bool CreditCardFormEventLogger::DoSuggestionsIncludeVirtualCard() {
  auto is_virtual_card = [](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kVirtualCreditCardEntry;
  };
  return std::ranges::any_of(suggestions_, is_virtual_card);
}

}  // namespace autofill::autofill_metrics
