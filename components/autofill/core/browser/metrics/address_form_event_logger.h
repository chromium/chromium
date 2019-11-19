// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_

#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"

namespace autofill {

class AddressFormEventLogger : public FormEventLoggerBase {
 public:
  AddressFormEventLogger(
      bool is_in_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

  ~AddressFormEventLogger() override;

  void OnDidFillSuggestion(const AutofillProfile& profile,
                           const FormStructure& form,
                           const AutofillField& field,
                           AutofillSyncSigninState sync_state);

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
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_
