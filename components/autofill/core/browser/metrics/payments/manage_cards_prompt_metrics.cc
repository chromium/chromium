// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill {

void LogManageCardsPromptMetric(ManageCardsPromptMetric metric,
                                bool is_upload_save) {
  std::string destination = is_upload_save ? ".Upload" : ".Local";
  std::string metric_with_destination =
      "Autofill.ManageCardsPrompt" + destination;
  base::UmaHistogramEnumeration(metric_with_destination, metric);
}

}  // namespace autofill
