// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_REMINDER_NOTICE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_REMINDER_NOTICE_METRICS_H_

namespace autofill::autofill_metrics {

// The user's interaction on the Wallet Reminder Notice bottom sheet or dialog.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WalletReminderNoticeInteraction)
enum class WalletReminderNoticeInteraction {
  // User clicked the primary "Got it" button.
  kAcknowledgedCta = 0,
  // User clicked a link in the legal disclaimer and navigated away.
  kClickedLink = 1,
  // User dismissed the notice by clicking outside or swiping down.
  kDismissed = 2,
  kMaxValue = kDismissed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillWalletReminderNoticeInteraction)

// The outcome of attempting to show the Wallet Reminder Notice.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WalletReminderNoticeShowResult)
enum class WalletReminderNoticeShowResult {
  // Notice was successfully displayed to the user.
  kShown = 0,
  // Not shown because the user already acknowledged the notice.
  kNotShownAlreadyAcknowledged = 1,
  // Not shown due to network or server error.
  kNotShownNetworkOrServerError = 2,
  // Not shown due to Mandatory Reauth.
  kNotShownDueToMandatoryReauth = 3,
  // Not shown due to VCN Enrollment.
  kNotShownDueToVcnEnrollment = 4,
  // Not shown due to card or CVC save.
  kNotShownDueToCardOrCvcSave = 5,
  kMaxValue = kNotShownDueToCardOrCvcSave,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillWalletReminderNoticeShowResult)

// Logs the user interaction with the Wallet Reminder Notice.
void LogWalletReminderNoticeInteraction(
    WalletReminderNoticeInteraction interaction);

// Logs the outcome of attempting to show the Wallet Reminder Notice.
void LogWalletReminderNoticeShowResult(
    WalletReminderNoticeShowResult show_result);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_REMINDER_NOTICE_METRICS_H_
