// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/task/single_thread_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

ACTION_P(ScheduleStatusCallback, status) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), status));
}

ACTION_P(ScheduleResultCallback, result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result));
}

class MockCloudPolicyClient : public CloudPolicyClient {
 public:
  MockCloudPolicyClient();
  explicit MockCloudPolicyClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  explicit MockCloudPolicyClient(DeviceManagementService* service);
  MockCloudPolicyClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceManagementService* service);
  MockCloudPolicyClient(const MockCloudPolicyClient&) = delete;
  MockCloudPolicyClient& operator=(const MockCloudPolicyClient&) = delete;
  ~MockCloudPolicyClient() override;

  MOCK_METHOD(void,
              SetupRegistration,
              (const std::string&,
               const std::string&,
               const std::vector<std::string>&),
              (override));
  MOCK_METHOD(void,
              Register,
              (const RegistrationParameters&,
               const std::string&,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              RegisterWithOidcResponse,
              (const RegistrationParameters&,
               const std::string&,
               const std::string&,
               const std::string&,
               const base::TimeDelta&,
               ResultCallback),
              (override));
  MOCK_METHOD(void, FetchPolicy, (PolicyFetchReason), (override));
  MOCK_METHOD(void,
              FetchRemoteCommands,
              (std::unique_ptr<RemoteCommandJob::UniqueIDType>,
               const std::vector<enterprise_management::RemoteCommandResult>&,
               enterprise_management::PolicyFetchRequest::SignatureType,
               const std::string&,
               RemoteCommandCallback),
              (override));
  MOCK_METHOD(void,
              UploadEnterpriseMachineCertificate,
              (const std::string&, ResultCallback),
              (override));
  MOCK_METHOD(void,
              UploadEnterpriseEnrollmentCertificate,
              (const std::string&, ResultCallback),
              (override));
  MOCK_METHOD(void,
              UploadEnterpriseEnrollmentId,
              (const std::string&, ResultCallback),
              (override));
  MOCK_METHOD(void,
              UploadDeviceStatus,
              (const enterprise_management::DeviceStatusReportRequest*,
               const enterprise_management::SessionStatusReportRequest*,
               const enterprise_management::ChildStatusReportRequest*,
               ResultCallback),
              (override));
  MOCK_METHOD(void, CancelAppInstallReportUpload, (), (override));
  MOCK_METHOD(void,
              UpdateGcmId,
              (const std::string&, StatusCallback),
              (override));
  MOCK_METHOD(void,
              UploadPolicyValidationReport,
              (CloudPolicyValidatorBase::Status,
               const std::vector<ValueValidationIssue>&,
               const ValidationAction,
               const std::string&,
               const std::string&),
              (override));
  MOCK_METHOD(
      void,
      UploadChromeDesktopReport,
      (std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>,
       ResultCallback),
      (override));
  MOCK_METHOD(
      void,
      UploadChromeOsUserReport,
      (std::unique_ptr<enterprise_management::ChromeOsUserReportRequest>,
       ResultCallback),
      (override));
  MOCK_METHOD(
      void,
      UploadChromeProfileReport,
      (std::unique_ptr<enterprise_management::ChromeProfileReportRequest>,
       ResultCallback),
      (override));
  MOCK_METHOD(void,
              UploadEuiccInfo,
              (std::unique_ptr<enterprise_management::UploadEuiccInfoRequest>,
               StatusCallback),
              (override));
  MOCK_METHOD(void,
              UploadSecurityEventReport,
              (bool, base::Value::Dict, ResultCallback),
              (override));
  MOCK_METHOD(void,
              UploadAppInstallReport,
              (base::Value::Dict value, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              ClientCertProvisioningRequest,
              (enterprise_management::ClientCertificateProvisioningRequest,
               ClientCertProvisioningRequestCallback),
              (override));
  MOCK_METHOD(void,
              UploadFmRegistrationToken,
              (enterprise_management::FmRegistrationTokenUploadRequest request,
               ResultCallback callback),
              (override));

  // Sets the DMToken.
  void SetDMToken(const std::string& token);

  // Injects policy.
  void SetPolicy(const std::string& policy_type,
                 const std::string& settings_entity_id,
                 const enterprise_management::PolicyFetchResponse& policy);

  // Inject invalidation version.
  void SetFetchedInvalidationVersion(int64_t fetched_invalidation_version);

  // Sets the status field.
  void SetStatus(DeviceManagementStatus status);

  // Make the notification helpers public.
  using CloudPolicyClient::NotifyClientError;
  using CloudPolicyClient::NotifyPolicyFetched;
  using CloudPolicyClient::NotifyRegistrationStateChanged;

  using CloudPolicyClient::client_id_;
  using CloudPolicyClient::dm_token_;
  using CloudPolicyClient::fetched_invalidation_version_;
  using CloudPolicyClient::invalidation_payload_;
  using CloudPolicyClient::invalidation_version_;
  using CloudPolicyClient::last_policy_timestamp_;
  using CloudPolicyClient::oidc_user_display_name_;
  using CloudPolicyClient::oidc_user_email_;
  using CloudPolicyClient::public_key_version_;
  using CloudPolicyClient::public_key_version_valid_;
  using CloudPolicyClient::third_party_identity_type_;
  using CloudPolicyClient::types_to_fetch_;
};

class MockCloudPolicyClientObserver : public CloudPolicyClient::Observer {
 public:
  MockCloudPolicyClientObserver();
  MockCloudPolicyClientObserver(const MockCloudPolicyClientObserver&) = delete;
  MockCloudPolicyClientObserver& operator=(
      const MockCloudPolicyClientObserver&) = delete;
  ~MockCloudPolicyClientObserver() override;

  MOCK_METHOD(void, OnPolicyFetched, (CloudPolicyClient*), (override));
  MOCK_METHOD(void,
              OnRegistrationStateChanged,
              (CloudPolicyClient*),
              (override));
  MOCK_METHOD(void, OnClientError, (CloudPolicyClient*), (override));
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
