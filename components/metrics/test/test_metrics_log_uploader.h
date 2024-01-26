// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_TEST_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_METRICS_TEST_TEST_METRICS_LOG_UPLOADER_H_

#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "third_party/metrics_proto/reporting_info.pb.h"

namespace metrics {

class TestMetricsLogUploader : public MetricsLogUploader {
 public:
  explicit TestMetricsLogUploader(
      const MetricsLogUploader::UploadCallback& on_upload_complete);

  TestMetricsLogUploader(const TestMetricsLogUploader&) = delete;
  TestMetricsLogUploader& operator=(const TestMetricsLogUploader&) = delete;

  ~TestMetricsLogUploader() override;

  // Mark the current upload complete with the given response code.
  void CompleteUpload(int response_code, bool force_discard = false);

  // Check if UploadLog has been called.
  bool is_uploading() const { return is_uploading_; }

  const ReportingInfo& reporting_info() const { return last_reporting_info_; }

  base::WeakPtr<TestMetricsLogUploader> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // MetricsLogUploader:
  void UploadLog(const std::string& compressed_log_data,
                 const LogMetadata& log_metadata,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const ReportingInfo& reporting_info) override;

  const MetricsLogUploader::UploadCallback on_upload_complete_;
  ReportingInfo last_reporting_info_;
  bool is_uploading_;
  base::WeakPtrFactory<TestMetricsLogUploader> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_TEST_METRICS_LOG_UPLOADER_H_
