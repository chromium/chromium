// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_LOYALTY_CARD_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_LOYALTY_CARD_FORM_EVENT_LOGGER_H_

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"

namespace autofill::autofill_metrics {

// To measure the readiness for loyalty cards by affiliation category.
enum class AffiliationCategoryMetricBucket {
  kNone = 0,
  kAffiliated = 1,
  kNonAffiliated = 2,
  kMixed = 3,
  kMaxValue = kMixed
};

class LoyaltyCardFormEventLogger : public FormEventLoggerBase {
 public:
  explicit LoyaltyCardFormEventLogger(BrowserAutofillManager* owner);

  ~LoyaltyCardFormEventLogger() override;

  void OnDidShowSuggestions(const FormStructure& form,
                            const AutofillField& field,
                            base::TimeTicks form_parsed_timestamp,
                            bool off_the_record,
                            base::span<const Suggestion> suggestions) override;

  // Triggered when the autofill manager fills a loyalty card suggestion.
  void OnDidFillSuggestion(const FormStructure& form,
                           const AutofillField& field,
                           const LoyaltyCard& loyalty_card,
                           const GURL& url);

  // Triggered when the list of loyalty card suggestions is loaded by the
  // autofill manager.
  void UpdateLoyaltyCardsAvailabilityForReadiness(
      const std::vector<LoyaltyCard>& loyalty_cards,
      const GURL& url);

 protected:
  void RecordParseForm() override;
  void RecordShowSuggestions() override;

  // Readiness, assistance, acceptance and correctness metrics resolved by card
  // category.
  void RecordFillingReadiness(LogBuffer& logs) const override;
  void RecordFillingAssistance(LogBuffer& logs) const override;
  void RecordFillingAcceptance(LogBuffer& logs) const override;
  void RecordFillingCorrectness(LogBuffer& logs) const override;

  void LogUkmInteractedWithForm(FormSignature form_signature) override;

  bool HasLoggedDataToFillAvailable() const override;
  DenseSet<FormTypeNameForLogging> GetSupportedFormTypeNamesForLogging()
      const override;
  DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
      const FormStructure& form) const override;

 private:
  size_t record_type_count_ = 0;
  // All card affiliation categories for which the user has at least one card
  // stored.
  DenseSet<LoyaltyCard::AffiliationCategory> card_categories_available_;
  // All card affiliation categories for which the user has accepted at least
  // one suggestion.
  DenseSet<LoyaltyCard::AffiliationCategory> card_categories_filled_;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_LOYALTY_CARD_FORM_EVENT_LOGGER_H_
