// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_H_

#include "base/functional/callback.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"

namespace enterprise_reporting {

// Defines the interface for uploading aggregated SaaS usage metrics.
// Implementations of this interface are responsible for the upload logic
// required to send SaasUsageReportEvent protos to the reporting server.
class SaasUsageReportUploader {
 public:
  virtual ~SaasUsageReportUploader() = default;

  // Uploads the given SaaS usage report. The `upload_callback` will be called
  // when the upload is complete with the upload success result.
  virtual void UploadReport(
      const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
      base::OnceCallback<void(bool)> upload_callback) = 0;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_H_
