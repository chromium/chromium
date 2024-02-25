// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyClient;

class POLICY_EXPORT RealtimeReportingJobConfiguration
    : public ReportingJobConfigurationBase {
 public:
  // Keys used in report dictionary.
  static const char kContextKey[];
  static const char kEventListKey[];

  // Keys used to parse the response.
  static const char kEventIdKey[];
  static const char kUploadedEventsKey[];
  static const char kFailedUploadsKey[];
  static const char kPermanentFailedUploadsKey[];

  // Combines the info given in |events| that corresponds to Event proto, and
  // info given in |context| that corresponds to the Device, Browser and Profile
  // proto, to a UploadEventsRequest proto defined in
  // google3/google/internal/chrome/reporting/v1/chromereporting.proto.
  static base::Value::Dict BuildReport(base::Value::List events,
                                       base::Value::Dict context);

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

  // Add a new report to the payload.  A report is a dictionary that
  // contains two keys: "events" and "context".  The first key is a list of
  // dictionaries, where dictionary is defined by the Event message described at
  // google/internal/chrome/reporting/v1/chromereporting.proto.
  //
  // The second is context information about this instance of chrome that
  // is not specific to the event.
  //
  // Returns true if the report was added successfully.
  bool AddReport(base::Value::Dict report);

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
  // Does one time initialization of the payload when the configuration is
  // created.
  void InitializePayloadInternal(CloudPolicyClient* client,
                                 bool include_device_info);

  // Gathers the ids of the uploads that failed
  std::set<std::string> GetFailedUploadIds(
      const std::string& response_body) const;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
