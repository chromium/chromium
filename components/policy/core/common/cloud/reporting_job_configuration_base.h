// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

class CloudPolicyClient;

// Base for common elements in JobConfigurations for the Reporting pipeline.
// Ensures the following elements are added to each request.
// Device dictionary:
// "device": {
//   "dmToken": "abcdef1234",
//   "clientId": "abcdef1234",
//   "osVersion": "10.0.0.0",
//   "osPlatform": "Windows",
//   "name": "George"
// }
//
// Browser dictionary:
// "browser": {
//   "browserId": "abcdef1234",
//   "chromeVersion": "10.0.0.0",
//   "machineUser": "abcdef1234"
// }
class POLICY_EXPORT ReportingJobConfigurationBase
    : public JobConfigurationBase {
 public:
  // Callback used once the job is complete.
  using UploadCompleteCallback =
      base::OnceCallback<void(DeviceManagementService::Job* job,
                              DeviceManagementStatus status,
                              int response_code,
                              absl::optional<base::Value::Dict>)>;

  // Builds a Device dictionary for uploading information about the device to
  // the server.
  class POLICY_EXPORT DeviceDictionaryBuilder {
   public:
    // Dictionary Key Name
    static const char kDeviceKey[];

    static base::Value::Dict BuildDeviceDictionary(
        const std::string& dm_token,
        const std::string& client_id);

    static std::string GetDMTokenPath();
    static std::string GetClientIdPath();
    static std::string GetOSVersionPath();
    static std::string GetOSPlatformPath();
    static std::string GetNamePath();

   private:
    static std::string GetStringPath(base::StringPiece leaf_name);

    // Keys used in Device dictionary.
    static const char kDMToken[];
    static const char kClientId[];
    static const char kOSVersion[];
    static const char kOSPlatform[];
    static const char kName[];
  };

  // Builds a Browser dictionary for uploading information about the browser to
  // the server.
  class POLICY_EXPORT BrowserDictionaryBuilder {
   public:
    // Dictionary Key Name
    static const char kBrowserKey[];

    static base::Value BuildBrowserDictionary(bool include_device_info);

    static std::string GetBrowserIdPath();
    static std::string GetUserAgentPath();
    static std::string GetMachineUserPath();
    static std::string GetChromeVersionPath();

   private:
    static std::string GetStringPath(base::StringPiece leaf_name);

    // Keys used in Browser dictionary.
    static const char kBrowserId[];
    static const char kUserAgent[];
    static const char kMachineUser[];
    static const char kChromeVersion[];
  };

  ReportingJobConfigurationBase(const ReportingJobConfigurationBase&) = delete;
  ReportingJobConfigurationBase& operator=(
      const ReportingJobConfigurationBase&) = delete;

  // DeviceManagementService::JobConfiguration
  std::string GetPayload() override;
  std::string GetUmaName() override;
  DeviceManagementService::Job::RetryMethod ShouldRetry(
      int response_code,
      const std::string& response_body) override;
  void OnBeforeRetry(int reponse_code,
                     const std::string& response_body) override;
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;
  GURL GetURL(int last_error) const override;

 protected:
  // |type| indicates which type of job.
  // |callback| will be called on upload completion.
  ReportingJobConfigurationBase(
      JobType type,
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      CloudPolicyClient* client,
      const std::string& server_url,
      bool include_device_info,
      UploadCompleteCallback callback);
  ~ReportingJobConfigurationBase() override;

  // Allows children to determine if a retry should be done.
  virtual DeviceManagementService::Job::RetryMethod ShouldRetryInternal(
      int response_code,
      const std::string& response_body);

  // Allows children to perform actions before a retry.
  virtual void OnBeforeRetryInternal(int response_code,
                                     const std::string& response_body);

  // Allows children to provide final mutations to |payload_| before completion
  // of |GetPayload| call.
  virtual void UpdatePayloadBeforeGetInternal();

  // Returns an identifying string for UMA.
  virtual std::string GetUmaString() const = 0;

  base::Value::Dict payload_;

  // Available to set additional fields by the child. An example of a context
  // being generated can be seen with the ::reporting::GetContext function. Once
  // |GetPayload| is called, |context_| will be merged into the payload and
  // reset.
  absl::optional<base::Value::Dict> context_;

  UploadCompleteCallback callback_;

 private:
  // Initializes request payload. If |include_device_info| is false, the
  // "device" and "browser.machineUser" fields (see comment at the top of the
  // file) are excluded from the payload.
  void InitializePayload(CloudPolicyClient* client, bool include_device_info);

  const std::string server_url_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_
