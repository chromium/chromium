// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
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

// Converts the VirtualCardEnrollmentRequestType to string to be used in
// histograms.
const char* GetVirtualCardEnrollmentRequestType(
    VirtualCardEnrollmentRequestType type) {
  switch (type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      return "Enroll";
    case VirtualCardEnrollmentRequestType::kUnenroll:
      return "Unenroll";
    case VirtualCardEnrollmentRequestType::kNone:
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

void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type) {
  base::UmaHistogramBoolean(
      base::JoinString(
          {"Autofill.VirtualCard", GetVirtualCardEnrollmentRequestType(type),
           "Attempt", GetVirtualCardEnrollmentSource(source)},
          "."),
      true);
}

void LogUpdateVirtualCardEnrollmentRequestResult(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type,
    bool succeeded) {
  base::UmaHistogramBoolean(
      base::JoinString(
          {"Autofill.VirtualCard", GetVirtualCardEnrollmentRequestType(type),
           "Result", GetVirtualCardEnrollmentSource(source)},
          "."),
      succeeded);
}

}  // namespace autofill
