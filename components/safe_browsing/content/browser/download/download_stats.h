// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_

#include <optional>

#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_stats.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

// The functions in this file are for logging UMA metrics related to downloads.
namespace safe_browsing {

// Enum representing different attributes for file types that are allowed on
// user gestures. This enum is used for logging UMA histograms, so entries must
// not be removed or reordered. Please update the enums.xml file if new values
// are added.
enum class UserGestureFileTypeAttributes {
  // The total number of checks. This value should be used as the denominator
  // when calculating the percentage of a specific attribute below.
  TOTAL_TYPE_CHECKED = 0,
  // The download is initiated with a user gesture.
  HAS_USER_GESTURE = 1,
  // The referrer of the download was visited before.
  HAS_REFERRER_VISIT = 2,
  // The referrer of the download is initiated with a user gesture and was
  // visited before.
  // The download is considered safe in this case.
  HAS_BOTH_USER_GESTURE_AND_REFERRER_VISIT = 3,
  // The user has bypassed download warnings before.
  HAS_BYPASSED_DOWNLOAD_WARNING = 4,

  kMaxValue = HAS_BYPASSED_DOWNLOAD_WARNING
};

// Records that a download warning was shown on the download shelf.
void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture);

// Records that a download warning was bypassed from the download shelf or the
// chrome://downloads page.
void RecordDangerousDownloadWarningBypassed(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture);

// Records the latency after completion a download was opened from the download
// shelf/bubble or the chrome://downloads page, or show in folder was clicked.
void RecordDownloadOpenedLatency(download::DownloadDangerType danger_type,
                                 download::DownloadContent download_content,
                                 base::Time download_opened_time,
                                 base::Time download_end_time,
                                 bool show_download_in_folder);

// Records the latency after completion for when a download was opened (via the
// shelf/bubble or chrome://downloads), or show in folder was clicked, by
// extension type.
void RecordDownloadOpenedLatencyFileType(
    download::DownloadContent download_content,
    base::Time download_opened_time,
    base::Time download_end_time);

// Records the attributes of a download.
void RecordDownloadFileTypeAttributes(
    DownloadFileType::DangerLevel danger_level,
    bool has_user_gesture,
    bool visited_referrer_before,
    std::optional<base::Time> latest_bypass_time);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
