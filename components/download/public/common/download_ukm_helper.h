// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Holds helpers for gathering UKM stats about downloads.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UKM_HELPER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UKM_HELPER_H_

#include <optional>

#include "components/download/public/common/download_content.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/resume_mode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace download {

class COMPONENTS_DOWNLOAD_EXPORT DownloadUkmHelper {
 public:
  DownloadUkmHelper() = delete;
  DownloadUkmHelper(const DownloadUkmHelper&) = delete;
  DownloadUkmHelper& operator=(const DownloadUkmHelper&) = delete;

  // Calculate which exponential bucket the value falls in. This is used to mask
  // the actual value of the metric due to privacy concerns for certain metrics
  // that could trace back the user's exact actions.
  static int CalcExponentialBucket(int value);

  // Calculate the number of bytes to the nearest kilobyte to maintain privacy.
  static int CalcNearestKB(int num_bytes);

  // Record when the download has started.
  static void RecordDownloadStarted(int download_id,
                                    ukm::SourceId source_id,
                                    DownloadContent file_type,
                                    DownloadSource download_source,
                                    DownloadConnectionSecurity state,
                                    bool is_same_host_download);

  // Record when the download is interrupted.
  static void RecordDownloadInterrupted(int download_id,
                                        std::optional<int> change_in_file_size,
                                        DownloadInterruptReason reason,
                                        int resulting_file_size,
                                        const base::TimeDelta& time_since_start,
                                        int64_t bytes_wasted);

  // Record when the download is resumed.
  static void RecordDownloadResumed(int download_id,
                                    ResumeMode mode,
                                    const base::TimeDelta& time_since_start);

  // Record when the download is completed.
  static void RecordDownloadCompleted(int download_id,
                                      int resulting_file_size,
                                      const base::TimeDelta& time_since_start,
                                      int64_t bytes_wasted);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UKM_HELPER_H_
