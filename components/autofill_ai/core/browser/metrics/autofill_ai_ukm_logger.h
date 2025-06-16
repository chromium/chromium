// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_METRICS_AUTOFILL_AI_UKM_LOGGER_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_METRICS_AUTOFILL_AI_UKM_LOGGER_H_

#include <cstddef>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill_ai {

// Utility to log URL keyed form interaction events for Autofill AI.
// Owned by AutofillAiLogger.
class AutofillAiUkmLogger {
 public:
  explicit AutofillAiUkmLogger(AutofillAiClient* autofill_client);
  ~AutofillAiUkmLogger();

  void LogKeyMetrics(ukm::SourceId ukm_source_id,
                     const autofill::FormStructure& form,
                     bool data_to_fill_available,
                     bool suggestions_shown,
                     bool suggestion_filled,
                     bool edited_autofilled_field,
                     bool opt_in_status);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // This represents the events for which we want to record metrics. It is
  // assumed that these events can only happen when the user is opted-in for
  // Autofill AI.
  enum class EventType {
    kSuggestionShown = 0,
    kSuggestionFilled = 1,
    kEditedAutofilledValue = 2,
    kFieldFilled = 3,
    kMaxValue = kFieldFilled
  };
  void LogFieldEvent(ukm::SourceId ukm_source_id,
                     const autofill::FormStructure& form,
                     const autofill::AutofillField& field,
                     EventType event_type);

 private:
  bool CanLogUkm(ukm::SourceId ukm_source_id) const;

  // Stores the number of FieldEvent's that were logged for each processed form.
  std::map<autofill::FormGlobalId, size_t> field_event_count_per_form_;

  // The top owning client. Note that the client owns the AutofillAiManager that
  // owns the AutofillAiLogger that owns this object.
  const raw_ref<AutofillAiClient> autofill_client_;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_METRICS_AUTOFILL_AI_UKM_LOGGER_H_
