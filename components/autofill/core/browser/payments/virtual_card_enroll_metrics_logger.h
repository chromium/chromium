// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLL_METRICS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLL_METRICS_LOGGER_H_

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"

namespace autofill {

// Logs metrics related to the virtual card enrollment.
class VirtualCardEnrollMetricsLogger {
 public:
  VirtualCardEnrollMetricsLogger();
  ~VirtualCardEnrollMetricsLogger();

  VirtualCardEnrollMetricsLogger(const VirtualCardEnrollMetricsLogger&) =
      delete;
  VirtualCardEnrollMetricsLogger& operator=(
      const VirtualCardEnrollMetricsLogger&) const = delete;

  VirtualCardEnrollMetricsLogger(VirtualCardEnrollMetricsLogger&&);
  VirtualCardEnrollMetricsLogger& operator=(VirtualCardEnrollMetricsLogger&&);

  // Called before the virtual card enrollment prompt is shown.
  //
  // `card_art_available` is true if the specific card art was available for
  // the card being enrolled.
  // `enrollment_bubble_source` is the original source of the enrollment
  // prompt.
  static void OnCardArtAvailable(bool card_art_available,
                                 VirtualCardEnrollmentSource enrollment_source);

  // Called once the virtual card enrollment prompt has been shown.
  //
  // `enrollment_bubble_source` is the original source of the enrollment
  // prompt.
  // `is_reshow` is whether the prompt was re-shown.
  static void OnShown(VirtualCardEnrollmentSource enrollment_source,
                      bool is_reshow);

  // Called when the prompt has been dismissed.
  //
  // If the prompt is dismissed by accepting or declining, then OnAccept or
  // OnDecline are called before OnDismiss.
  //
  // `enrollment_bubble_source` is the original source of the enrollment
  // prompt.
  // `is_reshow` is whether the prompt was re-shown. \param
  // `previously_declined` whether a previous prompt was declined.
  static void OnDismissed(VirtualCardEnrollmentBubbleResult result,
                          VirtualCardEnrollmentSource enrollment_source,
                          bool is_reshow,
                          bool previously_declined);

  // Called when a link (such as in issuer terms) has been opened (by clicking,
  // tapping, or activated through accessibility tools, etc.).
  //
  // `link_type` is the type of link clicked.
  // `enrollment_bubble_source` is the original source of the enrollment
  // prompt.
  static void OnLinkClicked(VirtualCardEnrollmentLinkType link_type,
                            VirtualCardEnrollmentSource enrollment_source);
};

// Returns an equivalent
// VirtualCardEnrollmentBubbleSource for the given
// VirtualCardEnrollmentSource.
VirtualCardEnrollmentBubbleSource ConvertToVirtualCardEnrollmentBubbleSource(
    VirtualCardEnrollmentSource enrollment_source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLL_METRICS_LOGGER_H_
