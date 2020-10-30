// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyClient;
class DMAuth;

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
                              DeviceManagementStatus code,
                              int net_error,
                              const base::Value&)>;

  // Builds a Device dictionary for uploading information about the device to
  // the server.
  class POLICY_EXPORT DeviceDictionaryBuilder {
   public:
    // Dictionary Key Name
    static const char kDeviceKey[];

    static base::Value BuildDeviceDictionary(const std::string& dm_token,
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

    static base::Value BuildBrowserDictionary();

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
  ReportingJobConfigurationBase(
      JobType type,
      std::unique_ptr<DMAuth> auth_data,
      base::Optional<std::string> oauth_token,
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      CloudPolicyClient* client,
      const std::string& server_url,
      UploadCompleteCallback callback);
  ~ReportingJobConfigurationBase() override;

  // Allows children to determine if a retry should be done.
  virtual DeviceManagementService::Job::RetryMethod ShouldRetryInternal(
      int response_code,
      const std::string& response_body);

  // Allows children to perform actions before a retry.
  virtual void OnBeforeRetryInternal(int response_code,
                                     const std::string& response_body);

  // Returns an identifying string for UMA.
  virtual std::string GetUmaString() const = 0;

  base::Value payload_;

 private:
  // Initializes request payload.
  void InitializePayload(CloudPolicyClient* client);

  const std::string server_url_;
  UploadCompleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ReportingJobConfigurationBase);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REPORTING_JOB_CONFIGURATION_BASE_H_
