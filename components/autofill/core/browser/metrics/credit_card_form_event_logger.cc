// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/validation.h"

namespace autofill {

CreditCardFormEventLogger::CreditCardFormEventLogger(
    bool is_in_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    PersonalDataManager* personal_data_manager,
    AutofillClient* client)
    : FormEventLoggerBase("CreditCard",
                          is_in_main_frame,
                          form_interactions_ukm_logger),
      personal_data_manager_(personal_data_manager),
      client_(client) {}

CreditCardFormEventLogger::~CreditCardFormEventLogger() = default;

void CreditCardFormEventLogger::OnDidSelectCardSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;

  // No need to log selections for local/full-server cards -- a selection is
  // always followed by a form fill, which is logged separately.
  if (credit_card.record_type() != CreditCard::MASKED_SERVER_CARD)
    return;

  Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, form);
  if (!has_logged_masked_server_card_suggestion_selected_) {
    has_logged_masked_server_card_suggestion_selected_ = true;
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, form);
  }
}

void CreditCardFormEventLogger::SetBankNameAvailable() {
  has_logged_bank_name_available_ = true;
}

void CreditCardFormEventLogger::OnDidFillSuggestion(
    const CreditCard& credit_card,
    const FormStructure& form,
    const AutofillField& field,
    AutofillSyncSigninState sync_state) {
  CreditCard::RecordType record_type = credit_card.record_type();
  sync_state_ = sync_state;

  form_interactions_ukm_logger_->LogDidFillSuggestion(
      record_type,
      /*is_for_credit_card=*/true, form, field);

  if (record_type == CreditCard::MASKED_SERVER_CARD)
    Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, form);
  else if (record_type == CreditCard::FULL_SERVER_CARD)
    Log(FORM_EVENT_SERVER_SUGGESTION_FILLED, form);
  else
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        record_type == CreditCard::MASKED_SERVER_CARD ||
        record_type == CreditCard::FULL_SERVER_CARD;
    logged_suggestion_filled_was_masked_server_card_ =
        record_type == CreditCard::MASKED_SERVER_CARD;
    if (record_type == CreditCard::MASKED_SERVER_CARD) {
      Log(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, form);
      if (has_logged_bank_name_available_) {
        Log(FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE);
      }
    } else if (record_type == CreditCard::FULL_SERVER_CARD) {
      Log(FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, form);
      if (has_logged_bank_name_available_) {
        Log(FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE);
      }
    } else {
      Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
    }
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledCreditCardSuggestion"));
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
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }
}

void CreditCardFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  form_interactions_ukm_logger_->LogInteractedWithForm(
      /*is_for_credit_card=*/true, local_record_type_count_,
      server_record_type_count_, form_signature);
}

void CreditCardFormEventLogger::OnSuggestionsShownOnce() {
  if (has_logged_bank_name_available_) {
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_BANK_NAME_AVAILABLE_ONCE);
  }
}

void CreditCardFormEventLogger::OnSuggestionsShownSubmittedOnce(
    const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    const CreditCard credit_card =
        client_->GetFormDataImporter()->ExtractCreditCardFromForm(form);
    Log(GetCardNumberStatusFormEvent(credit_card), form);
  }
}

void CreditCardFormEventLogger::OnLog(const std::string& name,
                                      FormEvent event,
                                      const FormStructure& form) const {
  // Log in a different histogram for credit card forms on nonsecure pages so
  // that form interactions on nonsecure pages can be analyzed on their own.
  if (!is_context_secure_) {
    base::UmaHistogramEnumeration(name + ".OnNonsecurePage", event,
                                  NUM_FORM_EVENTS);
  }
}

void CreditCardFormEventLogger::Log(BankNameDisplayedFormEvent event) const {
  DCHECK_LT(event, BANK_NAME_NUM_FORM_EVENTS);
  const std::string name("Autofill.FormEvents.CreditCard.BankNameDisplayed");
  base::UmaHistogramEnumeration(name, event, BANK_NAME_NUM_FORM_EVENTS);
}

FormEvent CreditCardFormEventLogger::GetCardNumberStatusFormEvent(
    const CreditCard& credit_card) {
  const base::string16 number = credit_card.number();
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

}  // namespace autofill
