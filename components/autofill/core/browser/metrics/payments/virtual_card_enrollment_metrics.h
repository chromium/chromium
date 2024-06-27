// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_

#include <string>

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace base {
class TimeDelta;
}

namespace autofill {

// Metrics to record user interaction with the virtual card enrollment
// bubble/infobar/bottomsheet.
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
  // The user cancelled the bubble.
  VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED = 5,
  kMaxValue = VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
};

// Metrics to record the source that prompted the virtual card enrollment
// bubble/infobar/bottomsheet.
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

// Used to determine the type of link that was clicked for logging purposes. A
// java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class VirtualCardEnrollmentLinkType {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // User selected the Google Payments terms of service link.
  VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK = 0,
  // User selected the issuer terms of service link.
  VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK = 1,
  // User selected the learn more about virtual cards link.
  VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK = 2,
  kMaxValue = VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
};

// Bubble shown and closed related metrics.
void LogVirtualCardEnrollmentBubbleShownMetric(
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow);
void LogVirtualCardEnrollmentBubbleResultMetric(
    VirtualCardEnrollmentBubbleResult result,
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow,
    bool previous_declined);

// Metrics to measure strikes logged or cleared in strike database.
enum class VirtualCardEnrollmentStrikeDatabaseEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Strike logged as enrollment bubble/infobar/bottomsheet was not accepted.
  VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKE_LOGGED = 0,
  // All strikes cleared as user accepted virtual card enrollment.
  VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKES_CLEARED = 1,
  kMaxValue = VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKES_CLEARED,
};

// GetDetailsForEnrollmentRequest related metrics. Attempts and results should
// be 1:1 mapping.
void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source);
void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded);
void LogGetDetailsForEnrollmentRequestLatency(
    VirtualCardEnrollmentSource source,
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    base::TimeDelta latency);

// UpdateVirtualCardEnrollmentRequest related metrics. Attempts and results
// should be 1:1 mapping.
void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type);
void LogUpdateVirtualCardEnrollmentRequestResult(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type,
    bool succeeded);

// Virtual card enrollment bubble/infobar/bottomsheet link clicked metrics.
void LogVirtualCardEnrollmentLinkClickedMetric(
    VirtualCardEnrollmentLinkType link_type,
    VirtualCardEnrollmentBubbleSource source);

// Virtual card enrollment strike database event metrics.
void LogVirtualCardEnrollmentStrikeDatabaseEvent(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentStrikeDatabaseEvent strike_event);

// Virtual card enrollment strike database max strikes limit reached metrics.
void LogVirtualCardEnrollmentBubbleMaxStrikesLimitReached(
    VirtualCardEnrollmentSource source);

// Virtual card enrollment bubble/infobar/bottomsheet card art available metric.
// Logs whether the card art was used in the enroll bubble/infobar/bottomsheet
// depending on if it was passed to the enrollment controller.
void LogVirtualCardEnrollBubbleCardArtAvailable(
    bool card_art_available,
    VirtualCardEnrollmentSource source);

// Latency Since Upstream metrics. Used to determine the time that it takes for
// the server calls that need to be made between Save Card Bubble accept and
// when the Virtual Card Enroll Bubble is shown.
void LogVirtualCardEnrollBubbleLatencySinceUpstream(base::TimeDelta latency);

// Logs the reason from strikedatabase perspective why virtual card enrollment
// is not offered.
void LogVirtualCardEnrollmentNotOfferedDueToMaxStrikes(
    VirtualCardEnrollmentSource source);
void LogVirtualCardEnrollmentNotOfferedDueToRequiredDelay(
    VirtualCardEnrollmentSource source);

// Logs whether the loading or confirmation views are shown.
void LogVirtualCardEnrollmentLoadingViewShown(bool is_shown);
void LogVirtualCardEnrollmentConfirmationViewShown(bool is_shown,
                                                   bool is_card_enrolled);

// Logs the loading or confirmation views results when the view is closed.
void LogVirtualCardEnrollmentLoadingViewResult(
    VirtualCardEnrollmentBubbleResult result);
void LogVirtualCardEnrollmentConfirmationViewResult(
    VirtualCardEnrollmentBubbleResult result,
    bool is_card_enrolled);

// Helper function used to convert VirtualCardEnrollmentBubbleSource enum to
// name suffix.
std::string VirtualCardEnrollmentBubbleSourceToMetricSuffix(
    VirtualCardEnrollmentBubbleSource source);

// Helper function used to convert VirtualCardEnrollmentSource enum to
// name suffix.
const std::string VirtualCardEnrollmentSourceToMetricSuffix(
    VirtualCardEnrollmentSource source);

// Helper function used to convert VirtualCardEnrollmentLinkType enum to
// name suffix.
const std::string VirtualCardEnrollmentLinkTypeToMetricSuffix(
    VirtualCardEnrollmentLinkType link_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
