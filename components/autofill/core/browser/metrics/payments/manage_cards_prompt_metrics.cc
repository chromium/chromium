// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill {

void LogManageCardsPromptMetric(ManageCardsPromptMetric metric) {
  base::UmaHistogramEnumeration("Autofill.ManageCardsPrompt", metric);
}

}  // namespace autofill
