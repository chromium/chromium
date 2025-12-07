// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/per_fill_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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
    size_t modified_fields_count,
    const FillingPayload& filling_payload) {
  constexpr const char prefix[] = "Autofill.NumberOfFieldsPerAutofill";
  std::string_view suffix = std::visit(
      absl::Overload{
          [&](const AutofillProfile*) { return "AutofillProfile"; },
          [&](const CreditCard* credit_card) { return "CreditCard"; },
          [&](const EntityInstance* entity) { return "EntityInstance"; },
          [&](const VerifiedProfile*) { return "VerifiedProfile"; },
          [&](const OtpFillData*) { return "OtpFillData"; }},
      filling_payload);
  base::UmaHistogramCounts1000(prefix, modified_fields_count);
  base::UmaHistogramCounts1000(base::StrCat({prefix, ".", suffix}),
                               modified_fields_count);
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
