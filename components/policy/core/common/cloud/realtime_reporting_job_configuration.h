// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/policy/policy_export.h"

namespace policy {


class CloudPolicyClient;

class POLICY_EXPORT RealtimeReportingJobConfiguration
    : public ReportingJobConfigurationBase {
 public:
  // Keys used to parse the response.
  static const char kEventIdKey[];
  static const char kUploadedEventsKey[];
  static const char kFailedUploadsKey[];
  static const char kPermanentFailedUploadsKey[];


  // Configures a request to send real-time reports to the |server_url|
  // endpoint. |callback| is invoked once the report is uploaded.
  RealtimeReportingJobConfiguration(CloudPolicyClient* client,
                                    const std::string& server_url,
                                    bool include_device_info,
                                    UploadCompleteCallback callback);
  RealtimeReportingJobConfiguration(const RealtimeReportingJobConfiguration&) =
      delete;
  RealtimeReportingJobConfiguration& operator=(
      const RealtimeReportingJobConfiguration&) = delete;

  ~RealtimeReportingJobConfiguration() override;

  // ReportingJobConfigurationBase.
  std::string GetPayload() override;

  std::string GetContentType() override;

  // Add a new Event proto to the payload.
  bool AddRequest(::chrome::cros::reporting::proto::UploadEventsRequest event);


 protected:
  // ReportingJobConfigurationBase
  DeviceManagementService::Job::RetryMethod ShouldRetryInternal(
      int response_code,
      const std::string& response) override;
  void OnBeforeRetryInternal(int response_code,
                             const std::string& response_body) override;

  bool ShouldRecordUma() const override;
  std::string GetUmaString() const override;

 private:
  void InitializeUploadRequest(CloudPolicyClient* client,
                               bool include_device_info);

  void OnUploadComplete(DeviceManagementService::Job* job,
                        DeviceManagementStatus status,
                        int response_code,
                        std::optional<base::DictValue> response);

  // The request to be sent to the server, use this instead of |payload_| for
  // realtime reporting.
  ::chrome::cros::reporting::proto::UploadEventsRequest upload_request_;

  UploadCompleteCallback complete_callback_;

  // Gathers the ids of the uploads that failed
  std::set<std::string> GetFailedUploadIds(
      const std::string& response_body) const;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
