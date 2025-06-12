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

class LoyaltyCardFormEventLogger : public FormEventLoggerBase {
 public:
  explicit LoyaltyCardFormEventLogger(BrowserAutofillManager* owner);

  ~LoyaltyCardFormEventLogger() override;

  // Triggered when the autofill manager fills a loyalty card suggestion.
  void OnDidFillSuggestion(const AutofillField& field);

  // Triggered when the list of loyalty card suggestions is loaded by the
  // autofill manager.
  void UpdateLoyaltyCardsAvailabilityForReadiness(
      const std::vector<LoyaltyCard>& loyalty_cards);

 protected:
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;

  void LogUkmInteractedWithForm(FormSignature form_signature) override;

  bool HasLoggedDataToFillAvailable() const override;
  DenseSet<FormTypeNameForLogging> GetSupportedFormTypeNamesForLogging()
      const override;
  DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
      const FormStructure& form) const override;

 private:
  size_t record_type_count_ = 0;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_LOYALTY_CARD_FORM_EVENT_LOGGER_H_
