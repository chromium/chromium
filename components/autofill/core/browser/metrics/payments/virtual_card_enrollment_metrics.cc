// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

namespace {

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

void LogVirtualCardEnrollmentBubbleShownMetric(
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow) {
  base::UmaHistogramBoolean(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentBubbleSourceToMetricSuffix(source),
      is_reshow);
}

void LogVirtualCardEnrollmentBubbleResultMetric(
    VirtualCardEnrollmentBubbleResult result,
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow,
    bool previously_declined) {
  std::string base_histogram_name =
      "Autofill.VirtualCardEnrollBubble.Result." +
      VirtualCardEnrollmentBubbleSourceToMetricSuffix(source) +
      (is_reshow ? ".Reshows" : ".FirstShow");
  base::UmaHistogramEnumeration(base_histogram_name, result);

  base::UmaHistogramEnumeration(
      base_histogram_name + (previously_declined ? ".WithPreviousStrikes"
                                                 : ".WithNoPreviousStrike"),
      result);
}

void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Attempt.",
                    VirtualCardEnrollmentSourceToMetricSuffix(source)}),
      true);
}

void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Result.",
                    VirtualCardEnrollmentSourceToMetricSuffix(source)}),
      succeeded);
}

void LogGetDetailsForEnrollmentRequestLatency(
    VirtualCardEnrollmentSource source,
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    base::TimeDelta latency) {
  base::UmaHistogramMediumTimes(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Latency." +
          VirtualCardEnrollmentSourceToMetricSuffix(source) +
          PaymentsRpcResultToMetricsSuffix(result),
      latency);
}

void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type) {
  base::UmaHistogramBoolean(
      base::JoinString(
          {"Autofill.VirtualCard", GetVirtualCardEnrollmentRequestType(type),
           "Attempt", VirtualCardEnrollmentSourceToMetricSuffix(source)},
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
           "Result", VirtualCardEnrollmentSourceToMetricSuffix(source)},
          "."),
      succeeded);
}

void LogVirtualCardEnrollmentLinkClickedMetric(
    VirtualCardEnrollmentLinkType link_type,
    VirtualCardEnrollmentBubbleSource source) {
  base::UmaHistogramBoolean(
      "Autofill.VirtualCardEnroll.LinkClicked." +
          VirtualCardEnrollmentBubbleSourceToMetricSuffix(source) + "." +
          VirtualCardEnrollmentLinkTypeToMetricSuffix(link_type),
      true);
}

void LogVirtualCardEnrollmentStrikeDatabaseEvent(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentStrikeDatabaseEvent strike_event) {
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardEnrollmentStrikeDatabase." +
          VirtualCardEnrollmentSourceToMetricSuffix(source),
      strike_event);
}

void LogVirtualCardEnrollmentBubbleMaxStrikesLimitReached(
    VirtualCardEnrollmentSource source) {
  DCHECK_NE(source, VirtualCardEnrollmentSource::kSettingsPage);
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardEnrollBubble.MaxStrikesLimitReached", source);
}

void LogVirtualCardEnrollBubbleCardArtAvailable(
    bool card_art_available,
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramBoolean(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable." +
          VirtualCardEnrollmentSourceToMetricSuffix(source),
      card_art_available);
}

void LogVirtualCardEnrollBubbleLatencySinceUpstream(base::TimeDelta latency) {
  base::UmaHistogramTimes(
      "Autofill.VirtualCardEnrollBubble.LatencySinceUpstream", latency);
}

void LogVirtualCardEnrollmentNotOfferedDueToMaxStrikes(
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase.VirtualCardEnrollmentNotOfferedDueToMaxStrikes",
      source);
}

void LogVirtualCardEnrollmentNotOfferedDueToRequiredDelay(
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase."
      "VirtualCardEnrollmentNotOfferedDueToRequiredDelay",
      source);
}

void LogVirtualCardEnrollmentLoadingViewShown(bool is_shown) {
  base::UmaHistogramBoolean("Autofill.VirtualCardEnrollBubble.LoadingShown",
                            is_shown);
}

void LogVirtualCardEnrollmentConfirmationViewShown(bool is_shown,
                                                   bool is_card_enrolled) {
  std::string_view base_histogram_name =
      "Autofill.VirtualCardEnrollBubble.ConfirmationShown";
  std::string_view is_card_enrolled_name =
      is_card_enrolled ? ".CardEnrolled" : ".CardNotEnrolled";

  base::UmaHistogramBoolean(
      base::StrCat({base_histogram_name, is_card_enrolled_name}), is_shown);
}

void LogVirtualCardEnrollmentLoadingViewResult(
    VirtualCardEnrollmentBubbleResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardEnrollBubble.LoadingResult", result);
}

void LogVirtualCardEnrollmentConfirmationViewResult(
    VirtualCardEnrollmentBubbleResult result,
    bool is_card_enrolled) {
  std::string_view base_histogram_name =
      "Autofill.VirtualCardEnrollBubble.ConfirmationResult";
  std::string_view is_card_enrolled_name =
      is_card_enrolled ? ".CardEnrolled" : ".CardNotEnrolled";

  base::UmaHistogramEnumeration(
      base::StrCat({base_histogram_name, is_card_enrolled_name}), result);
}

std::string VirtualCardEnrollmentBubbleSourceToMetricSuffix(
    VirtualCardEnrollmentBubbleSource source) {
  switch (source) {
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_UNKNOWN_SOURCE:
      return "Unknown";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE:
      return "Upstream";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_DOWNSTREAM_SOURCE:
      return "Downstream";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE:
      return "SettingsPage";
  }
}

const std::string VirtualCardEnrollmentLinkTypeToMetricSuffix(
    VirtualCardEnrollmentLinkType link_type) {
  switch (link_type) {
    case VirtualCardEnrollmentLinkType::
        VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK:
      return "GoogleLegalMessageLink";
    case VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK:
      return "IssuerLegalMessageLink";
    case VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK:
      return "LearnMoreLink";
  }
}

const std::string VirtualCardEnrollmentSourceToMetricSuffix(
    VirtualCardEnrollmentSource source) {
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

}  // namespace autofill
