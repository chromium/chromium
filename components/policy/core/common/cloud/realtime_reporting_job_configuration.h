// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyClient;
class DMAuth;

class POLICY_EXPORT RealtimeReportingJobConfiguration
    : public JobConfigurationBase {
 public:
  // Keys used in report dictionary.
  static const char kContextKey[];
  static const char kEventListKey[];

  // Keys used in request payload dictionary.  Public for testing.
  static const char kBrowserIdKey[];
  static const char kChromeVersionKey[];
  static const char kClientIdKey[];
  static const char kDmTokenKey[];
  static const char kEventsKey[];
  static const char kMachineUserKey[];
  static const char kOsVersionKey[];

  typedef base::OnceCallback<void(DeviceManagementService::Job* job,
                                  DeviceManagementStatus code,
                                  int net_error,
                                  const base::Value&)>
      Callback;

  // Combines the info given in |events| that corresponds to Event proto, and
  // info given in |context| that corresponds to the Device, Browser and Profile
  // proto, to a UploadEventsRequest proto defined in
  // google3/google/internal/chrome/reporting/v1/chromereporting.proto.
  static base::Value BuildReport(base::Value events, base::Value context);

  RealtimeReportingJobConfiguration(CloudPolicyClient* client,
                                    std::unique_ptr<DMAuth> auth_data,
                                    Callback callback);

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
  bool AddReport(base::Value report);

 private:
  // Does one time initialization of the payload when the configuration is
  // created.
  void InitializePayload(CloudPolicyClient* client);

  // DeviceManagementService::JobConfiguration.
  std::string GetPayload() override;
  std::string GetUmaName() override;
  void OnBeforeRetry() override {}
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  // JobConfigurationBase overrides.
  GURL GetURL(int last_error) override;

  std::string server_url_;
  base::Value payload_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(RealtimeReportingJobConfiguration);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
