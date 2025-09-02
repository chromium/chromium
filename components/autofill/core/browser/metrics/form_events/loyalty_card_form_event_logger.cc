// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/loyalty_card_form_event_logger.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "url/gurl.h"

namespace autofill::autofill_metrics {
namespace {

using AffiliationCategory = LoyaltyCard::AffiliationCategory;

// Converts a set of `AffiliationCategory` to the corresponding
// `AffiliationCategoryMetricBucket`.
AffiliationCategoryMetricBucket AffiliationCategoriesToMetricBucket(
    DenseSet<AffiliationCategory> affiliation_categories) {
  if (affiliation_categories.empty()) {
    return AffiliationCategoryMetricBucket::kNone;
  }
  if (affiliation_categories.size() > 1u) {
    return AffiliationCategoryMetricBucket::kMixed;
  }
  switch (*affiliation_categories.begin()) {
    case AffiliationCategory::kNonAffiliated:
      return AffiliationCategoryMetricBucket::kNonAffiliated;
    case AffiliationCategory::kAffiliated:
      return AffiliationCategoryMetricBucket::kAffiliated;
  }
}

// Returns the histogram suffix used for a set of affiliation categories.
std::string_view GetAffiliationCategoriesSuffix(
    DenseSet<AffiliationCategory> affiliation_categories) {
  CHECK_GT(affiliation_categories.size(), 0ul);
  if (affiliation_categories.size() > 1) {
    return "Mixed";
  }
  switch (*affiliation_categories.begin()) {
    case AffiliationCategory::kNonAffiliated:
      return "NonAffiliated";
    case AffiliationCategory::kAffiliated:
      return "Affiliated";
  }
}

}  // namespace

LoyaltyCardFormEventLogger::LoyaltyCardFormEventLogger(
    BrowserAutofillManager* owner)
    : FormEventLoggerBase("LoyaltyCard", owner) {}

LoyaltyCardFormEventLogger::~LoyaltyCardFormEventLogger() = default;

void LoyaltyCardFormEventLogger::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record,
    base::span<const Suggestion> suggestions) {
  FormEventLoggerBase::OnDidShowSuggestions(
      form, field, field.Type().GetLoyaltyCardType(), form_parsed_timestamp,
      off_the_record, suggestions);
}

void LoyaltyCardFormEventLogger::UpdateLoyaltyCardsAvailabilityForReadiness(
    const std::vector<LoyaltyCard>& loyalty_cards,
    const GURL& url) {
  record_type_count_ = loyalty_cards.size();
  card_categories_available_.clear();
  for (const LoyaltyCard& loyalty_card : loyalty_cards) {
    card_categories_available_.insert(loyalty_card.GetAffiliationCategory(url));
  }
}

void LoyaltyCardFormEventLogger::OnDidFillSuggestion(
    const FormStructure& form,
    const AutofillField& field,
    const LoyaltyCard& loyalty_card,
    const GURL& url) {
  client().GetFormInteractionsUkmLogger().LogDidFillSuggestion(
      driver().GetPageUkmSourceId(), form, field,
      /*record_type=*/std::nullopt);
  Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
  if (!has_logged_form_filling_suggestion_filled_) {
    has_logged_form_filling_suggestion_filled_ = true;
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
  }
  FieldType field_type = field.Type().GetLoyaltyCardType();
  field_types_with_shown_suggestions_.erase(field_type);
  field_types_with_accepted_suggestions_.insert(field_type);
  ++form_interaction_counts_.autofill_fills;

  card_categories_filled_.insert(loyalty_card.GetAffiliationCategory(url));
}

void LoyaltyCardFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedLoyaltyCardForm"));
}

void LoyaltyCardFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedLoyaltyCardSuggestions"));
}

void LoyaltyCardFormEventLogger::RecordFillingReadiness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingReadiness(logs);
  base::UmaHistogramEnumeration(
      "Autofill.LoyaltyCard.FillingReadinessAffiliationCategory",
      AffiliationCategoriesToMetricBucket(card_categories_available_));
}

void LoyaltyCardFormEventLogger::RecordFillingAssistance(
    LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAssistance(logs);
  // In addition record assistance for affiliated cards. Notably,
  // assistance is recorded only if affiliated cards are available.
  if (card_categories_available_.contains(AffiliationCategory::kAffiliated)) {
    base::UmaHistogramBoolean(
        "Autofill.LoyaltyCard.FillingAssistance.Affiliated",
        card_categories_filled_.contains(AffiliationCategory::kAffiliated));
  }
}

void LoyaltyCardFormEventLogger::RecordFillingAcceptance(
    LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAcceptance(logs);
  // In addition record acceptance for affiliated cards. Notably,
  // acceptance is recorded only if affiliated cards are available.
  if (card_categories_available_.contains(AffiliationCategory::kAffiliated)) {
    base::UmaHistogramBoolean(
        "Autofill.LoyaltyCard.FillingAcceptance.Affiliated",
        card_categories_filled_.contains(AffiliationCategory::kAffiliated));
  }
}

void LoyaltyCardFormEventLogger::RecordFillingCorrectness(
    LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingCorrectness(logs);
  // Non-empty because correctness is only logged when an Autofill suggestion
  // was accepted.
  CHECK(!card_categories_filled_.empty());
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.LoyaltyCard.FillingCorrectness.",
                    GetAffiliationCategoriesSuffix(card_categories_filled_)}),
      !has_logged_edited_autofilled_field_);
}

void LoyaltyCardFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  // Loyalty card Autofill doesn't have the concept of server loyalty cards.
  client().GetFormInteractionsUkmLogger().LogInteractedWithForm(
      driver().GetPageUkmSourceId(),
      /*is_for_credit_card=*/false, record_type_count_,
      /*server_record_type_count=*/0, form_signature);
}

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
