// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_

#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill::autofill_metrics {

// To measure the added value of kAccount profiles, the filling assistance
// metric is split by profile category. Since the metric is emitted at
// navigation (not at filling time), multiple profiles can be used to fill a
// single form. This is represented by kMixed.
enum class CategoryResolvedFillingAssistanceBucket {
  kNone = 0,
  kLocalOrSyncable = 1,
  kAccountChrome = 2,
  kAccountNonChrome = 3,
  kMixed = 4,
  kMaxValue = kMixed
};

class AddressFormEventLogger : public FormEventLoggerBase {
 public:
  AddressFormEventLogger(
      bool is_in_any_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      AutofillClient* client);

  ~AddressFormEventLogger() override;

  void OnDidFillSuggestion(const AutofillProfile& profile,
                           const FormStructure& form,
                           const AutofillField& field,
                           AutofillSyncSigninState sync_state,
                           const AutofillTriggerSource trigger_source);

  void OnDidSeeFillableDynamicForm(AutofillSyncSigninState sync_state,
                                   const FormStructure& form);

  void OnDidRefill(AutofillSyncSigninState sync_state,
                   const FormStructure& form);

  void OnSubsequentRefillAttempt(AutofillSyncSigninState sync_state,
                                 const FormStructure& form);

 protected:
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;
  void OnLog(const std::string& name,
             FormEvent event,
             const FormStructure& form) const override;

  // Assistance and correctness metrics resolved by profile category.
  void RecordFillingAssistance(LogBuffer& logs) const override;
  void RecordFillingCorrectness(LogBuffer& logs) const override;

 private:
  // All profile categories for which the user has accepted at least one
  // suggestion - used for metrics.
  DenseSet<AutofillProfileSourceCategory> profile_categories_filled_;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
