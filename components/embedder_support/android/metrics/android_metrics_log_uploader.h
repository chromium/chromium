// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_LOG_UPLOADER_H_

#include <string>

#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

// Uploads UMA logs using the platform logging mechanism.
class AndroidMetricsLogUploader : public MetricsLogUploader {
 public:
  explicit AndroidMetricsLogUploader(
      const MetricsLogUploader::UploadCallback& on_upload_complete);

  ~AndroidMetricsLogUploader() override;

  AndroidMetricsLogUploader(const AndroidMetricsLogUploader&) = delete;
  AndroidMetricsLogUploader& operator=(const AndroidMetricsLogUploader&) =
      delete;

  // MetricsLogUploader:
  // Note: |log_hash| and |log_signature| are only used by the normal UMA
  // server. This uploader uses a Java logging mechanism that ignores these
  // fields.
  void UploadLog(const std::string& compressed_log_data,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const ReportingInfo& reporting_info) override;

 private:
  const MetricsLogUploader::UploadCallback on_upload_complete_;
};

}  // namespace metrics

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_LOG_UPLOADER_H_
