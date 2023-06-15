// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MandatoryReauthOptInBubbleOffer {
  // The user is shown the opt-in bubble.
  kShown = 0,
  kMaxValue = kShown,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MandatoryReauthOptInBubbleResult {
  // The reason why the bubble is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 0,
  // The user explicitly accepted the bubble by clicking the ok button.
  kAccepted = 1,
  // The user explicitly cancelled the bubble by clicking the cancel button.
  kCancelled = 2,
  // The user explicitly closed the bubble with the close button or ESC.
  kClosed = 3,
  // The user did not interact with the bubble.
  kNotInteracted = 4,
  // The bubble lost focus and was deactivated.
  kLostFocus = 5,
  kMaxValue = kLostFocus,
};

// Logs when the user is offered mandatory reauth.
void LogMandatoryReauthOptInBubbleOffer(MandatoryReauthOptInBubbleOffer metric,
                                        bool is_reshow);

// Logs when the user interacts with the opt-in bubble.
void LogMandatoryReauthOptInBubbleResult(
    MandatoryReauthOptInBubbleResult metric,
    bool is_reshow);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_
