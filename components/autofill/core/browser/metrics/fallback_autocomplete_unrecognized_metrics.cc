// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"

#include <string_view>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void AutocompleteUnrecognizedFallbackMetricLogger::ContextMenuEntryShown(
    bool field_has_ac_unrecognized) {
  CHECK_EQ(state_, State::kFallbackNotShown);
  state_ = field_has_ac_unrecognized
               ? State::kShownOnAutocompleteUnrecognizedField
               : State::kShownOnAutocompleteRecognizedField;
}

void AutocompleteUnrecognizedFallbackMetricLogger::ContextMenuEntryAccepted() {
  CHECK_NE(state_, State::kFallbackNotShown);
  was_accepted_ = true;
}

void AutocompleteUnrecognizedFallbackMetricLogger::ContextMenuClosed() {
  if (state_ == State::kFallbackNotShown) {
    return;
  }

  auto metric_name = [](std::string_view bucket) {
    return base::StrCat(
        {"Autofill.ManualFallback.ExplicitlyTriggered.", bucket, ".Address"});
  };
  // Emit to the bucket corresponding to the `state` and to the "Total" bucket.
  base::UmaHistogramBoolean(
      metric_name(state_ == State::kShownOnAutocompleteRecognizedField
                      ? "ClassifiedFieldAutocompleteRecognized"
                      : "ClassifiedFieldAutocompleteUnrecognized"),
      was_accepted_);
  base::UmaHistogramBoolean(metric_name("Total"), was_accepted_);
}

}  // namespace autofill::autofill_metrics
