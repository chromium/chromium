// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/strings/string_piece.h"

namespace metrics {

class ReportingInfo;

// MetricsLogUploader is an abstract base class for uploading UMA logs on behalf
// of MetricsService.
class MetricsLogUploader {
 public:
  // Type for OnUploadComplete callbacks. These callbacks will receive five
  // parameters:
  //   - a response code,
  //   - a net error code,
  //   - a boolean specifying if the connection was secure (over HTTPS),
  //   - a boolean specifying if the log should be discarded regardless of
  //     response/error code,
  //   - a string specifying the reason why the log was forcibly discarded (or
  //     empty string if not).
  using UploadCallback =
      base::RepeatingCallback<void(int, int, bool, bool, base::StringPiece)>;

  // Possible service types. This should correspond to a type from
  // DataUseUserData.
  // TODO(crbug.com/1445151) Investigate cleaning up this enum if it isn't
  // needed anymore.
  enum MetricServiceType {
    UMA,
    UKM,
    STRUCTURED_METRICS,
  };

  virtual ~MetricsLogUploader() = default;

  // Uploads a log with the specified |compressed_log_data|, a |log_hash| and
  // |log_signature| for data validation, and |reporting_info|. |log_hash| is
  // expected to be the hex-encoded SHA1 hash of the log data before compression
  // and |log_signature| is expected to be a base64-encoded HMAC-SHA256
  // signature of the log data before compression. When the server receives an
  // upload it recomputes the hash and signature of the upload and compares it
  // to the ones inlcuded in the upload. If there is a missmatched, the upload
  // is flagged. If an Uploader implementation uploads to a server that doesn't
  // do this validation then |log_hash| and |log_signature| can be ignored.
  virtual void UploadLog(const std::string& compressed_log_data,
                         const std::string& log_hash,
                         const std::string& log_signature,
                         const ReportingInfo& reporting_info) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_UPLOADER_H_
