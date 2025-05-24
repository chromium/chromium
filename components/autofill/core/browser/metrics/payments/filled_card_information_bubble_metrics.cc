// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/filled_card_information_bubble_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogFilledCardInformationBubbleShown(bool is_reshow) {
  base::UmaHistogramBoolean("Autofill.FilledCardInformationBubble.Shown",
                            is_reshow);
}

void LogFilledCardInformationBubbleResultMetric(
    FilledCardInformationBubbleResult metric,
    bool is_reshow) {
  static const char kFirstShow[] =
      "Autofill.FilledCardInformationBubble.Result.FirstShow";
  static const char kReshows[] =
      "Autofill.FilledCardInformationBubble.Result.Reshows";
  base::UmaHistogramEnumeration(is_reshow ? kReshows : kFirstShow, metric);
}

void LogFilledCardInformationBubbleFieldClicked(
    FilledCardInformationBubbleFieldClicked metric) {
  base::UmaHistogramEnumeration(
      "Autofill.FilledCardInformationBubble.FieldClicked", metric);
}

}  // namespace autofill::autofill_metrics
