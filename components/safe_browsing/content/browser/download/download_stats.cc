// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/download/download_stats.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/download/public/common/download_content.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace {

using safe_browsing::FileTypePolicies;

constexpr char kShownMetricSuffix[] = ".Shown";
constexpr char kBypassedMetricSuffix[] = ".Bypassed";

std::string GetDangerTypeMetricSuffix(
    download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return ".DangerousFileType";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return ".Malicious";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return ".Uncommon";
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      return ".Others";
  }
}

void RecordDangerousWarningFileType(download::DownloadDangerType danger_type,
                                    const base::FilePath& file_path,
                                    const std::string& metrics_suffix) {
  base::UmaHistogramSparse(
      "SBClientDownload.Warning.FileType" +
          GetDangerTypeMetricSuffix(danger_type) + metrics_suffix,
      FileTypePolicies::GetInstance()->UmaValueForFile(file_path));
}

void RecordDangerousWarningIsHttps(download::DownloadDangerType danger_type,
                                   bool is_https,
                                   const std::string& metrics_suffix) {
  base::UmaHistogramBoolean("SBClientDownload.Warning.DownloadIsHttps" +
                                GetDangerTypeMetricSuffix(danger_type) +
                                metrics_suffix,
                            is_https);
}

void RecordDangerousWarningHasUserGesture(
    download::DownloadDangerType danger_type,
    bool has_user_gesture,
    const std::string& metrics_suffix) {
  base::UmaHistogramBoolean("SBClientDownload.Warning.DownloadHasUserGesture" +
                                GetDangerTypeMetricSuffix(danger_type) +
                                metrics_suffix,
                            has_user_gesture);
}

}  // namespace

namespace safe_browsing {

void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture) {
  RecordDangerousWarningFileType(danger_type, file_path, kShownMetricSuffix);
  RecordDangerousWarningIsHttps(danger_type, is_https, kShownMetricSuffix);
  RecordDangerousWarningHasUserGesture(danger_type, has_user_gesture,
                                       kShownMetricSuffix);
  base::RecordAction(
      base::UserMetricsAction("SafeBrowsing.Download.WarningShown"));
}

void RecordDangerousDownloadWarningBypassed(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture) {
  RecordDangerousWarningFileType(danger_type, file_path, kBypassedMetricSuffix);
  RecordDangerousWarningIsHttps(danger_type, is_https, kBypassedMetricSuffix);
  RecordDangerousWarningHasUserGesture(danger_type, has_user_gesture,
                                       kBypassedMetricSuffix);
  base::RecordAction(
      base::UserMetricsAction("SafeBrowsing.Download.WarningBypassed"));
}

void RecordDownloadOpenedLatency(download::DownloadDangerType danger_type,
                                 download::DownloadContent download_content,
                                 base::Time download_opened_time,
                                 base::Time download_end_time,
                                 bool show_download_in_folder) {
  if (danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    return;
  }
  std::string metric_suffix =
      show_download_in_folder ? ".ShowInFolder" : ".OpenDirectly";
  base::UmaHistogramCustomTimes(
      "SBClientDownload.SafeDownloadOpenedLatency2" + metric_suffix,
      /* sample */ download_opened_time - download_end_time,
      /* min */ base::Seconds(1),
      /* max */ base::Days(1), /* buckets */ 50);
}

void RecordDownloadFileTypeAttributes(
    DownloadFileType::DangerLevel danger_level,
    bool has_user_gesture,
    bool visited_referrer_before,
    std::optional<base::Time> last_bypass_time) {
  if (danger_level != DownloadFileType::ALLOW_ON_USER_GESTURE) {
    return;
  }
  base::UmaHistogramEnumeration(
      "SBClientDownload.UserGestureFileType.Attributes",
      UserGestureFileTypeAttributes::TOTAL_TYPE_CHECKED);
  if (has_user_gesture) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_USER_GESTURE);
  }
  if (visited_referrer_before) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_REFERRER_VISIT);
  }
  if (has_user_gesture && visited_referrer_before) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::
            HAS_BOTH_USER_GESTURE_AND_REFERRER_VISIT);
  }
  if (last_bypass_time) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_BYPASSED_DOWNLOAD_WARNING);
  }
}

}  // namespace safe_browsing
