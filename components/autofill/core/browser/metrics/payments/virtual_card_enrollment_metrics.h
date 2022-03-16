// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_

#include <string>

#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace base {
class TimeDelta;
}

namespace autofill {

// Metrics to record user interaction with the virtual card enrollment bubble.
enum class VirtualCardEnrollmentBubbleResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The reason why the bubble is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_RESULT_UNKNOWN = 0,
  // The user accepted the bubble.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED = 1,
  // The user explicitly closed the bubble with the close button or ESC.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED = 2,
  // The user did not interact with the bubble.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED = 3,
  // The bubble lost focus and was deactivated.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS = 4,
  kMaxValue = VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS,
};

// Metrics to record the source that prompted the virtual card enrollment
// bubble.
enum class VirtualCardEnrollmentBubbleSource {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The source of the bubble is unknown.
  VIRTUAL_CARD_ENROLLMENT_UNKNOWN_SOURCE = 0,
  // Bubble began after an unenrolled VCN-eligible card was saved via credit
  // card upload save (Upstream).
  VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE = 1,
  // Bubble began after an unenrolled VCN-eligible card was unmasked during a
  // checkout flow (Downstream).
  VIRTUAL_CARD_ENROLLMENT_DOWNSTREAM_SOURCE = 2,
  // Bubble began when the enrollment is started by user in Chrome settings
  // page.
  VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE = 3,
  kMaxValue = VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE,
};

// Bubble shown and closed related metrics.
void LogVirtualCardEnrollmentBubbleShownMetric(
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow);
void LogVirtualCardEnrollmentBubbleResultMetric(
    VirtualCardEnrollmentBubbleResult result,
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow);

// GetDetailsForEnrollmentRequest related metrics. Attempts and results should
// be 1:1 mapping.
void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source);
void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded);

// UpdateVirtualCardEnrollmentRequest related metrics. Attempts and results
// should be 1:1 mapping.
void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type);
void LogUpdateVirtualCardEnrollmentRequestResult(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type,
    bool succeeded);

// Helper function used to convert VirtualCardEnrollmentBubbleSource enum to
// name suffix.
std::string VirtualCardEnrollmentBubbleSourceToMetricSuffix(
    VirtualCardEnrollmentBubbleSource source);

// Helper function used to convert VirtualCardEnrollmentSource enum to
// name suffix.
const std::string VirtualCardEnrollmentSourceToMetricSuffix(
    VirtualCardEnrollmentSource source);

// Latency Since Upstream metrics. Used to determine the time that it takes for
// the server calls that need to be made between Save Card Bubble accept and
// when the Virtual Card Enroll Bubble is shown.
void LogVirtualCardEnrollBubbleLatencySinceUpstream(
    const base::TimeDelta& latency);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
