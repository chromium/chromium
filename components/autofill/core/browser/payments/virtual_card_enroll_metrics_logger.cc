// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

VirtualCardEnrollMetricsLogger::VirtualCardEnrollMetricsLogger() = default;
VirtualCardEnrollMetricsLogger::~VirtualCardEnrollMetricsLogger() = default;

VirtualCardEnrollMetricsLogger::VirtualCardEnrollMetricsLogger(
    VirtualCardEnrollMetricsLogger&&) = default;
VirtualCardEnrollMetricsLogger& VirtualCardEnrollMetricsLogger::operator=(
    VirtualCardEnrollMetricsLogger&&) = default;

void VirtualCardEnrollMetricsLogger::OnCardArtAvailable(
    bool card_art_available,
    VirtualCardEnrollmentSource enrollment_source) {
  LogVirtualCardEnrollBubbleCardArtAvailable(card_art_available,
                                             enrollment_source);
}

void VirtualCardEnrollMetricsLogger::OnShown(
    VirtualCardEnrollmentSource enrollment_source,
    bool is_reshow) {
  LogVirtualCardEnrollmentBubbleShownMetric(
      ConvertToVirtualCardEnrollmentBubbleSource(enrollment_source), is_reshow);
}

void VirtualCardEnrollMetricsLogger::OnLinkClicked(
    VirtualCardEnrollmentLinkType link_type,
    VirtualCardEnrollmentSource enrollment_source) {
  LogVirtualCardEnrollmentLinkClickedMetric(
      link_type, ConvertToVirtualCardEnrollmentBubbleSource(enrollment_source));
}

void VirtualCardEnrollMetricsLogger::OnDismissed(
    VirtualCardEnrollmentBubbleResult result,
    VirtualCardEnrollmentSource enrollment_source,
    bool is_reshow,
    bool previously_declined) {
  LogVirtualCardEnrollmentBubbleResultMetric(
      result, ConvertToVirtualCardEnrollmentBubbleSource(enrollment_source),
      is_reshow, previously_declined);
}

VirtualCardEnrollmentBubbleSource ConvertToVirtualCardEnrollmentBubbleSource(
    VirtualCardEnrollmentSource enrollment_source) {
  switch (enrollment_source) {
    case VirtualCardEnrollmentSource::kUpstream:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE;
    case VirtualCardEnrollmentSource::kDownstream:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_DOWNSTREAM_SOURCE;
    case VirtualCardEnrollmentSource::kSettingsPage:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED();
  }
}

}  // namespace autofill
