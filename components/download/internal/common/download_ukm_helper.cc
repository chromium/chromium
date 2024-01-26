// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_ukm_helper.h"

#include "base/numerics/safe_conversions.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace download {

namespace {
// Return the "size" of the bucket based on the allowed percent_error.
double CalcBucketIncrement() {
  double percent_error = 10;
  return log10(1 + percent_error / 100);
}
}  // namespace

int DownloadUkmHelper::CalcExponentialBucket(int value) {
  return base::saturated_cast<int>(
      floor(log10(value + 1) / CalcBucketIncrement()));
}

int DownloadUkmHelper::CalcNearestKB(int num_bytes) {
  return num_bytes / 1024;
}

void DownloadUkmHelper::RecordDownloadStarted(int download_id,
                                              ukm::SourceId source_id,
                                              DownloadContent file_type,
                                              DownloadSource download_source,
                                              DownloadConnectionSecurity state,
                                              bool is_same_host_download) {
  ukm::builders::Download_Started(source_id)
      .SetDownloadId(download_id)
      .SetFileType(static_cast<int>(file_type))
      .SetDownloadSource(static_cast<int>(download_source))
      .SetDownloadConnectionSecurity(static_cast<int>(state))
      .SetIsSameHostDownload(is_same_host_download)
      .Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadInterrupted(
    int download_id,
    std::optional<int> change_in_file_size,
    DownloadInterruptReason reason,
    int resulting_file_size,
    const base::TimeDelta& time_since_start,
    int64_t bytes_wasted) {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::Download_Interrupted builder(source_id);
  builder.SetDownloadId(download_id)
      .SetReason(static_cast<int>(reason))
      .SetResultingFileSize(
          DownloadUkmHelper::CalcExponentialBucket(resulting_file_size))
      .SetTimeSinceStart(time_since_start.InMilliseconds())
      .SetBytesWasted(DownloadUkmHelper::CalcNearestKB(bytes_wasted));
  if (change_in_file_size.has_value()) {
    builder.SetChangeInFileSize(
        DownloadUkmHelper::CalcExponentialBucket(change_in_file_size.value()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadResumed(
    int download_id,
    ResumeMode mode,
    const base::TimeDelta& time_since_start) {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::Download_Resumed(source_id)
      .SetDownloadId(download_id)
      .SetMode(static_cast<int>(mode))
      .SetTimeSinceStart(time_since_start.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadCompleted(
    int download_id,
    int resulting_file_size,
    const base::TimeDelta& time_since_start,
    int64_t bytes_wasted) {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::Download_Completed(source_id)
      .SetDownloadId(download_id)
      .SetResultingFileSize(
          DownloadUkmHelper::CalcExponentialBucket(resulting_file_size))
      .SetTimeSinceStart(time_since_start.InMilliseconds())
      .SetBytesWasted(DownloadUkmHelper::CalcNearestKB(bytes_wasted))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace download
