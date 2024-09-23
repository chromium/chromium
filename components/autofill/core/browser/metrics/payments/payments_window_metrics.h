// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_WINDOW_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_WINDOW_METRICS_H_

#include "base/time/time.h"

namespace autofill::autofill_metrics {

// Metrics to track the flow events for a VCN 3DS authentication flow.
// Ideally, where enum buckets are concerned:
//
// Buckets 1+2+3 should approximately add up to bucket 0 (the flow started
// bucket), because any time the flow starts, the automatic next step is either
// the consent dialog being skipped, the user declining the consent dialog, or
// the user accepting the consent dialog.
//
// Buckets 3+4+5+6+7+8+9 should approximately add up to bucket 0 (the flow
// started bucket) because these buckets are related to events that terminate
// the flow.
//
// These values should add up approximately because the user closing the tab or
// browser will not be logged in this histogram. So any slight discrepancies
// will be due to that.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Vcn3dsFlowEvent {
  // The flow has started, and the consent dialog or a pop-up are about to be
  // shown.
  kFlowStarted = 0,
  // The consent dialog was skipped, as consent was given previously.
  kUserConsentDialogSkipped = 1,
  // The consent dialog was accepted.
  kUserConsentDialogAccepted = 2,
  // The consent dialog was declined.
  kUserConsentDialogDeclined = 3,
  // The pop-up was not able to be shown when a show was attempted.
  kPopupNotShown = 4,
  // The user cancelled the flow by closing the pop-up before authentication has
  // finished.
  kFlowCancelledUserClosedPopup = 5,
  // The authentication inside of the pop-up failed.
  kAuthenticationInsidePopupFailed = 6,
  // The user cancelled the progress dialog while retrieving the VCN.
  kProgressDialogCancelled = 7,
  // The flow failed after pop-up closed while retrieving the VCN.
  kFlowFailedWhileRetrievingVCN = 8,
  // The flow has successfully completed and retrieved a VCN.
  kFlowSucceeded = 9,
  kMaxValue = kFlowSucceeded,
};

// Enum to track the result of a corresponding PaymentsWindowUserConsentDialog
// that was shown.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PaymentsWindowUserConsentDialogResult {
  // The tab or browser was closed.
  kTabOrBrowserClosed = 0,
  // The escape key was pressed, closing the dialog.
  kEscapeKeyPressed = 1,
  // The cancel button was pressed, closing the dialog.
  kCancelButtonClicked = 2,
  // The accept button was pressed, closing the dialog.
  kAcceptButtonClicked = 3,
  kMaxValue = kAcceptButtonClicked,
};

// Logs the flow event for a VCN 3DS authentication.
void LogVcn3dsFlowEvent(Vcn3dsFlowEvent flow_event,
                        bool user_consent_already_given);

// Logs events related to the PaymentsWindowUserConsentDialog.
void LogPaymentsWindowUserConsentDialogResult(
    PaymentsWindowUserConsentDialogResult result);

// Logs when a PaymentsWindowUserConsentDialog is shown.
void LogPaymentsWindowUserConsentDialogShown();

// Logs the duration that a VCN 3DS auth flow took. `duration` is the time
// between when the pop-up was displayed to the user, and when the pop-up
// closed. `success` is whether the auth was a success or failure.
void LogVcn3dsAuthLatency(base::TimeDelta duration, bool success);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_WINDOW_METRICS_H_
