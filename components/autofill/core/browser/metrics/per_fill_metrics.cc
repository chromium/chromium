// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/per_fill_metrics.h"

#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/filling/form_filler.h"

namespace autofill::autofill_metrics {

namespace {
std::string RefillTriggerReasonToString(
    RefillTriggerReason refill_trigger_reason) {
  switch (refill_trigger_reason) {
    case RefillTriggerReason::kFormChanged:
      return "FormChanged";
    case RefillTriggerReason::kSelectOptionsChanged:
      return "SelectOptionsChanged";
    case RefillTriggerReason::kExpirationDateFormatted:
      return "ExpirationDateFormatted";
  }
  NOTREACHED();
}
}  // namespace

void LogNumberOfFieldsModifiedByAutofill(
    base::span<const FormFieldData*> safe_filled_fields,
    const FillingPayload& filling_payload) {
  constexpr const char prefix[] = "Autofill.NumberOfFieldsPerAutofill";
  std::string_view suffix = std::visit(
      base::Overloaded{
          [&](const AutofillProfile*) { return "AutofillProfile"; },
          [&](const CreditCard* credit_card) { return "CreditCard"; },
          [&](const EntityInstance* entity) { return "EntityInstance"; },
          [&](const VerifiedProfile*) { return "VerifiedProfile"; }},
      filling_payload);
  base::UmaHistogramCounts1000(prefix, safe_filled_fields.size());
  base::UmaHistogramCounts1000(base::StrCat({prefix, ".", suffix}),
                               safe_filled_fields.size());
}

void LogRefillTriggerReason(RefillTriggerReason refill_trigger_reason) {
  base::UmaHistogramEnumeration("Autofill.RefillTriggerReason",
                                refill_trigger_reason);
}

void LogNumberOfFieldsModifiedByRefill(
    RefillTriggerReason refill_trigger_reason,
    size_t num_modified_fields) {
  base::UmaHistogramCounts100(
      base::StrCat({"Autofill.Refill.ModifiedFieldsCount.",
                    RefillTriggerReasonToString(refill_trigger_reason)}),
      num_modified_fields);
}

}  // namespace autofill::autofill_metrics
