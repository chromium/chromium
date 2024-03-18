// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/granular_filling_metrics_utils.h"

#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

namespace {

// Gets the last `FillingMethod` logged for a `field`. If the filled was
// never filled using Autofill, returns `FillingMethod::kNone`.
// `field_log_events()` returns all log events that were added to a field in
// chronological order, so in order to know the last `FillingMethod`
// used for `field` we can look for the latest log event in the array of type
// `FillFieldLogEvent` that has a its filling method different to
// `FillingMethod::kNone`.
FillingMethod GetLastFieldFillingMethod(const AutofillField& field) {
  for (const auto& log_event : base::Reversed(field.field_log_events())) {
    if (auto* event = absl::get_if<FillFieldLogEvent>(&log_event)) {
      switch (event->filling_method) {
        case FillingMethod::kFullForm:
          return FillingMethod::kFullForm;
        case FillingMethod::kGroupFillingName:
        case FillingMethod::kGroupFillingAddress:
        case FillingMethod::kGroupFillingEmail:
        case FillingMethod::kGroupFillingPhoneNumber:
          // For metric purposes, we do not care about the type of group filling
          // that was used.
          return FillingMethod::kGroupFillingAddress;
        case FillingMethod::kFieldByFieldFilling:
          return FillingMethod::kFieldByFieldFilling;
        case FillingMethod::kNone:
          break;
      }
    }
  }
  return FillingMethod::kNone;
}

}  // namespace

std::string_view FillingMethodToCompactStringView(
    FillingMethod filling_method) {
  switch (filling_method) {
    case FillingMethod::kFullForm:
      return "FullForm";
    case FillingMethod::kGroupFillingName:
    case FillingMethod::kGroupFillingAddress:
    case FillingMethod::kGroupFillingEmail:
    case FillingMethod::kGroupFillingPhoneNumber:
      return "GroupFilling";
    case FillingMethod::kFieldByFieldFilling:
      return "FieldByFieldFilling";
    case FillingMethod::kNone:
      return "None";
  }
}

void AddFillingStatsForFillingMethod(
    const AutofillField& field,
    base::flat_map<FillingMethod, autofill_metrics::FormGroupFillingStats>&
        field_stats_by_filling_method) {
  const FillingMethod filling_method = GetLastFieldFillingMethod(field);
  auto filling_stats_form_autofill_filling_method_it =
      field_stats_by_filling_method.find(filling_method);

  // If this is not the first field that has a certain `FillingMethod`,
  // update the existing entry to include the `field`s `FieldFillingStatus`.
  // Otherwise create a new entry in the map for the field`s filling method and
  // set the `FormGroupFillingStats` for the `field`s `FieldFillingStatus` as
  // value.
  if (filling_stats_form_autofill_filling_method_it !=
      field_stats_by_filling_method.end()) {
    filling_stats_form_autofill_filling_method_it->second.AddFieldFillingStatus(
        GetFieldFillingStatus(field));
  } else {
    FormGroupFillingStats filling_method_stats;
    filling_method_stats.AddFieldFillingStatus(GetFieldFillingStatus(field));
    field_stats_by_filling_method[filling_method] = filling_method_stats;
  }
}

}  // namespace autofill::autofill_metrics
