// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/loyalty_card_form_event_logger.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"

namespace autofill::autofill_metrics {

// TODO(crbug.com/422366498): Complete the class implementation.
LoyaltyCardFormEventLogger::LoyaltyCardFormEventLogger(
    BrowserAutofillManager* owner)
    : FormEventLoggerBase("LoyaltyCard", owner) {}

LoyaltyCardFormEventLogger::~LoyaltyCardFormEventLogger() = default;

void LoyaltyCardFormEventLogger::UpdateLoyaltyCardsAvailabilityForReadiness(
    const std::vector<LoyaltyCard>& loyalty_cards) {
  record_type_count_ = loyalty_cards.size();
}

void LoyaltyCardFormEventLogger::OnDidFillSuggestion(
    const FormStructure& form,
    const AutofillField& field) {
  Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
  if (!has_logged_form_filling_suggestion_filled_) {
    has_logged_form_filling_suggestion_filled_ = true;
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
  }
  FieldType field_type = field.Type().GetStorableType();
  field_types_with_shown_suggestions_.erase(field_type);
  field_types_with_accepted_suggestions_.insert(field_type);
  ++form_interaction_counts_.autofill_fills;
}

void LoyaltyCardFormEventLogger::RecordPollSuggestions() {}

void LoyaltyCardFormEventLogger::RecordParseForm() {}

void LoyaltyCardFormEventLogger::RecordShowSuggestions() {}

void LoyaltyCardFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {}

bool LoyaltyCardFormEventLogger::HasLoggedDataToFillAvailable() const {
  return record_type_count_ > 0;
}

DenseSet<FormTypeNameForLogging>
LoyaltyCardFormEventLogger::GetSupportedFormTypeNamesForLogging() const {
  return {FormTypeNameForLogging::kLoyaltyCardForm};
}

DenseSet<FormTypeNameForLogging>
LoyaltyCardFormEventLogger::GetFormTypesForLogging(
    const FormStructure& form) const {
  return GetLoyaltyFormTypesForLogging(form);
}

}  // namespace autofill::autofill_metrics
