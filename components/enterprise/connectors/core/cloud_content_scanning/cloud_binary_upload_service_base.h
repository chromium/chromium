// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_

#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"

namespace enterprise_connectors {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
//
// TODO(crbug.com/501456247): Change this to inherit from BinaryUploadService
// once the migration is done.
class CloudBinaryUploadServiceBase {
 public:
  explicit CloudBinaryUploadServiceBase();
  virtual ~CloudBinaryUploadServiceBase();

  // The maximum number of uploads that can happen in parallel.
  static size_t GetParallelActiveRequestsMax();

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

  // Returns true if all expected connector results (tags) have been received
  // for the given `request_id`.
  bool ResponseIsComplete(BinaryUploadRequest::Id request_id);

  // Returns the request associated with `request_id`, or nullptr if it doesn't
  // exist in `active_requests_`.
  BinaryUploadRequest* GetRequest(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  // TODO(crbug.com/501456247): Change this from protected to private once the
  // migration is done.
 protected:
  // Records UMA metrics for a finished request.
  void RecordRequestMetrics(BinaryUploadRequest::Id request_id,
                            ScanRequestUploadResult result);

  // Records UMA metrics for a finished request, including tag-specific results.
  void RecordRequestMetrics(
      BinaryUploadRequest::Id request_id,
      ScanRequestUploadResult result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Resources associated with an in-progress request.
  base::flat_map<BinaryUploadRequest::Id, std::unique_ptr<BinaryUploadRequest>>
      active_requests_;

  // Maps request IDs to their start times, used for duration metrics.
  base::flat_map<BinaryUploadRequest::Id, base::TimeTicks> start_times_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<
      BinaryUploadRequest::Id,
      base::flat_map<std::string,
                     enterprise_connectors::ContentAnalysisResponse::Result>>
      received_connector_results_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
