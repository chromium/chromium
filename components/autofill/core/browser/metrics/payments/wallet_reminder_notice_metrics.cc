// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogWalletReminderNoticeInteraction(
    WalletReminderNoticeInteraction interaction) {
  base::UmaHistogramEnumeration("Autofill.WalletReminderNotice.Interaction",
                                interaction);
}

void LogWalletReminderNoticeShowResult(
    WalletReminderNoticeShowResult show_result) {
  base::UmaHistogramEnumeration("Autofill.WalletReminderNotice.ShowResult",
                                show_result);
}

}  // namespace autofill::autofill_metrics
