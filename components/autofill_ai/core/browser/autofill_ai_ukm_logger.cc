// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_ukm_logger.h"

#include <cstdint>

#include "base/check_deref.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/signatures.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill_ai {

AutofillAiUkmLogger::AutofillAiUkmLogger(AutofillAiClient* autofill_client)
    : autofill_client_(CHECK_DEREF(autofill_client)) {}

AutofillAiUkmLogger::~AutofillAiUkmLogger() = default;

void AutofillAiUkmLogger::LogKeyMetrics(ukm::SourceId ukm_source_id,
                                        const autofill::FormStructure& form,
                                        bool data_to_fill_available,
                                        bool suggestions_shown,
                                        bool edited_autofilled_field,
                                        bool suggestion_filled,
                                        bool opt_in_status) {
  if (!CanLog(ukm_source_id)) {
    return;
  }

  ukm::builders::AutofillAi_KeyMetrics builder(ukm_source_id);
  builder.SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFormSessionIdentifier(
          autofill::autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()))
      .SetFillingReadiness(data_to_fill_available)
      .SetFillingAssistance(suggestion_filled)
      .SetOptInStatus(opt_in_status);
  if (suggestions_shown) {
    builder.SetFillingAcceptance(suggestion_filled);
  }
  if (suggestion_filled) {
    builder.SetFillingCorrectness(!edited_autofilled_field);
  }
  builder.Record(autofill_client_->GetAutofillClient().GetUkmRecorder());
}

void AutofillAiUkmLogger::LogFieldEvent(ukm::SourceId ukm_source_id,
                                        const autofill::FormStructure& form,
                                        const autofill::AutofillField& field,
                                        EventType event_type) {
  if (!CanLog(ukm_source_id)) {
    return;
  }

  ukm::builders::AutofillAi_FieldEvent(ukm_source_id)
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFormSessionIdentifier(
          autofill::autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()))
      .SetFormSessionEventOrder(field_event_count_per_form_[form.global_id()]++)
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldSessionIdentifier(
          autofill::autofill_metrics::FieldGlobalIdToHash64Bit(
              field.global_id()))
      .SetFormatStringSource(base::to_underlying(field.format_string_source()))
      .SetFieldType(base::to_underlying(field.Type().GetStorableType()))
      .SetAiFieldType(base::to_underlying(
          field.GetAutofillAiServerTypePredictions().value_or(
              autofill::UNKNOWN_TYPE)))
      .SetEventType(base::to_underlying(event_type))
      .Record(autofill_client_->GetAutofillClient().GetUkmRecorder());
}

bool AutofillAiUkmLogger::CanLog(ukm::SourceId ukm_source_id) const {
  return autofill_client_->GetAutofillClient().GetUkmRecorder() != nullptr &&
         ukm_source_id != ukm::kInvalidSourceId;
}

}  // namespace autofill_ai
