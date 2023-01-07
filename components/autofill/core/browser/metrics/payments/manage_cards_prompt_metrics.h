// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANAGE_CARDS_PROMPT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANAGE_CARDS_PROMPT_METRICS_H_

namespace autofill {

// Metrics to measure user interaction with the Manage Cards view shown when
// user clicks on the save card icon after accepting to save a card.
enum class ManageCardsPromptMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The manage cards promo was shown.
  kManageCardsShown = 0,
  // The user clicked on [Done].
  kManageCardsDone = 1,
  // The user clicked on [Manage cards].
  kManageCardsManageCards = 2,
  kMaxValue = kManageCardsManageCards
};

void LogManageCardsPromptMetric(ManageCardsPromptMetric metric,
                                bool is_upload_save);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANAGE_CARDS_PROMPT_METRICS_H_
