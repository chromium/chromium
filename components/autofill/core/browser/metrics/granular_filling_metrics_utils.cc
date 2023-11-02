// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/granular_filling_metrics_utils.h"

#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

namespace {

// Gets the last `AutofillFillingMethod` logged for a `field`. If the filled was
// never filled using Autofill, returns `AutofillFillingMethod::kNone`.
// `field_log_events()` returns all log events that were added to a field in
// chronological order, so in order to know the last `AutofillFillingMethod`
// used for `field` we can look for the latest log event in the array of type
// `FillFieldLogEvent` that has a its filling method different to
// `AutofillFillingMethod::kNone`.
AutofillFillingMethod GetLastFieldAutofillFillingMethod(
    const AutofillField& field) {
  for (const auto& log_event : base::Reversed(field.field_log_events())) {
    auto* event = absl::get_if<FillFieldLogEvent>(&log_event);
    if (event && event->filling_method != AutofillFillingMethod::kNone) {
      return event->filling_method;
    }
  }
  return AutofillFillingMethod::kNone;
}

}  // namespace

base::StringPiece AutofillFillingMethodToStringPiece(
    AutofillFillingMethod filling_method) {
  switch (filling_method) {
    case AutofillFillingMethod::kFullForm:
      return "FullForm";
    case AutofillFillingMethod::kGroupFilling:
      return "GroupFilling";
    case AutofillFillingMethod::kFieldByFieldFilling:
      return "FieldByFieldFilling";
    case AutofillFillingMethod::kNone:
      return "None";
  }
}

void AddFillingStatsForAutofillFillingMethod(
    const AutofillField& field,
    base::flat_map<AutofillFillingMethod,
                   autofill_metrics::FormGroupFillingStats>&
        field_stats_by_filling_method) {
  const AutofillFillingMethod filling_method =
      GetLastFieldAutofillFillingMethod(field);
  auto filling_stats_form_autofill_filling_method_it =
      field_stats_by_filling_method.find(filling_method);

  // If this is not the first field that has a certain `AutofillFillingMethod`,
  // update the existing entry to include the `field`s `FieldFillingStatus`.
  // Otherwise create a new entry in the map for the field`s filling method and
  // set the `FormGroupFillingStats` for the `field`s `FieldFillingStatus` as
  // value.
  if (filling_stats_form_autofill_filling_method_it !=
      field_stats_by_filling_method.end()) {
    filling_stats_form_autofill_filling_method_it->second.AddFieldFillingStatus(
        autofill_metrics::GetFieldFillingStatus(field));
  } else {
    autofill_metrics::FormGroupFillingStats filling_method_stats;
    filling_method_stats.AddFieldFillingStatus(
        autofill_metrics::GetFieldFillingStatus(field));
    field_stats_by_filling_method[filling_method] = filling_method_stats;
  }
}

}  // namespace autofill::autofill_metrics
