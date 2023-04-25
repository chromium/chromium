// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/web_content_handler.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"

namespace {

constexpr char kLocalWebApprovalDurationHistogramName[] =
    "FamilyLinkUser.LocalWebApprovalCompleteRequestTotalDuration";
constexpr char kLocalWebApprovalResultHistogramName[] =
    "FamilyLinkUser.LocalWebApprovalResult";

// Records the outcome of the local web approval flow.
void RecordLocalWebApprovalResultMetric(
    supervised_user::WebContentHandler::LocalApprovalResult result) {
  base::UmaHistogramEnumeration(kLocalWebApprovalResultHistogramName, result);
}

// Records the duration of a complete local web approval flow.
void RecordTimeToApprovalDurationMetric(base::TimeDelta durationMs) {
  base::UmaHistogramLongTimes(kLocalWebApprovalDurationHistogramName,
                              durationMs);
}

std::string LocalApprovalResultToString(
    supervised_user::WebContentHandler::LocalApprovalResult value) {
  switch (value) {
    case supervised_user::WebContentHandler::LocalApprovalResult::kApproved:
      return "Approved";
    case supervised_user::WebContentHandler::LocalApprovalResult::kDeclined:
      return "Rejected";
    case supervised_user::WebContentHandler::LocalApprovalResult::kCanceled:
      return "Incomplete";
    case supervised_user::WebContentHandler::LocalApprovalResult::kError:
      return "Error";
  }
}

}  // namespace

namespace supervised_user {

WebContentHandler::WebContentHandler() = default;

WebContentHandler::~WebContentHandler() = default;

void WebContentHandler::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    LocalApprovalResult approval_result) {
  VLOG(0) << "Local URL approval final result: "
          << LocalApprovalResultToString(approval_result);

  switch (approval_result) {
    case LocalApprovalResult::kApproved:
      settings_service.RecordLocalWebsiteApproval(url.host());
      // Record duration metrics only for completed approval flows.
      RecordTimeToApprovalDurationMetric(base::TimeTicks::Now() - start_time);
      break;
    case LocalApprovalResult::kDeclined:
      // Record duration metrics only for completed approval flows.
      RecordTimeToApprovalDurationMetric(base::TimeTicks::Now() - start_time);
      break;
    case LocalApprovalResult::kCanceled:
      break;
    case LocalApprovalResult::kError:
      break;
  }
  RecordLocalWebApprovalResultMetric(approval_result);
}

// static
const char* WebContentHandler::GetLocalApprovalDurationMillisecondsHistogram() {
  return kLocalWebApprovalDurationHistogramName;
}

// static
const char* WebContentHandler::GetLocalApprovalResultHistogram() {
  return kLocalWebApprovalResultHistogramName;
}

}  // namespace supervised_user
