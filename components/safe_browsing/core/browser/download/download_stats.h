// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_

#include "components/download/public/common/download_danger_type.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

// The functions in this file are for logging UMA metrics related to downloads.
namespace safe_browsing {

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

// Records that a download was opened from the download shelf or the
// chrome://downloads page.
void RecordDownloadOpened(download::DownloadDangerType danger_type,
                          base::Time download_opened_time,
                          base::Time download_end_time,
                          bool show_download_in_folder);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
