// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/test_metrics_log_uploader.h"
#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

TestMetricsLogUploader::TestMetricsLogUploader(
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : on_upload_complete_(on_upload_complete), is_uploading_(false) {}

TestMetricsLogUploader::~TestMetricsLogUploader() = default;

void TestMetricsLogUploader::CompleteUpload(int response_code,
                                            bool force_discard) {
  DCHECK(is_uploading_);
  is_uploading_ = false;
  last_reporting_info_.Clear();
  on_upload_complete_.Run(response_code, /*error_code=*/0, /*was_https=*/false,
                          force_discard, /*force_discard_reason=*/"");
}

void TestMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                       const LogMetadata& log_metadata,
                                       const std::string& log_hash,
                                       const std::string& log_signature,
                                       const ReportingInfo& reporting_info) {
  DCHECK(!is_uploading_);
  is_uploading_ = true;
  last_reporting_info_ = reporting_info;
}

}  // namespace metrics
