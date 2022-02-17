// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

namespace {

// Converts the VirtualCardEnrollmentSource to string to be used in histograms.
const char* GetVirtualCardEnrollmentSource(VirtualCardEnrollmentSource source) {
  switch (source) {
    case VirtualCardEnrollmentSource::kUpstream:
      return "Upstream";
    case VirtualCardEnrollmentSource::kDownstream:
      return "Downstream";
    case VirtualCardEnrollmentSource::kSettingsPage:
      return "SettingsPage";
    case VirtualCardEnrollmentSource::kNone:
      return "Unknown";
  }
}

}  // namespace

void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Attempt.",
                    GetVirtualCardEnrollmentSource(source)}),
      true);
}

void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Result.",
                    GetVirtualCardEnrollmentSource(source)}),
      succeeded);
}

}  // namespace autofill
