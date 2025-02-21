// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/web_content_handler.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace {

// Records the duration of a complete local web approval flow.
void RecordTimeToApprovalDurationMetric(base::TimeDelta durationMs) {
  base::UmaHistogramLongTimes(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName,
      durationMs);
}

std::string LocalApprovalResultToString(
    supervised_user::LocalApprovalResult value) {
  switch (value) {
    case supervised_user::LocalApprovalResult::kApproved:
      return "Approved";
    case supervised_user::LocalApprovalResult::kDeclined:
      return "Rejected";
    case supervised_user::LocalApprovalResult::kCanceled:
      return "Incomplete";
    case supervised_user::LocalApprovalResult::kError:
      return "Error";
  }
}

void MaybeRecordLocalWebApprovalErrorTypeMetric(
    std::optional<supervised_user::LocalWebApprovalErrorType> error_type) {
  if (!error_type.has_value()) {
    return;
  }
  base::UmaHistogramEnumeration(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, error_type.value());
}
}  // namespace

namespace supervised_user {

WebContentHandler::WebContentHandler() = default;

WebContentHandler::~WebContentHandler() = default;

void WebContentHandler::RecordLocalWebApprovalResultMetric(
    LocalApprovalResult result) {
  base::UmaHistogramEnumeration(kLocalWebApprovalResultHistogramName, result);
}

void WebContentHandler::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    LocalApprovalResult approval_result,
    std::optional<supervised_user::LocalWebApprovalErrorType>
        local_approval_error_type) {
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
      MaybeRecordLocalWebApprovalErrorTypeMetric(local_approval_error_type);
      break;
  }
  RecordLocalWebApprovalResultMetric(approval_result);
}

// static
const char* WebContentHandler::GetLocalApprovalDurationMillisecondsHistogram() {
  return kLocalWebApprovalDurationMillisecondsHistogramName;
}

// static
const char* WebContentHandler::GetLocalApprovalResultHistogram() {
  return kLocalWebApprovalResultHistogramName;
}

}  // namespace supervised_user
