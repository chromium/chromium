// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char* JobTypeToRequestType(
    DeviceManagementService::JobConfiguration::JobType type) {
  switch (type) {
    case DeviceManagementService::JobConfiguration::TYPE_INVALID:
      NOTREACHED() << "Not a DMServer request type" << type;
      return "Invalid";
    case DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT:
      return dm_protocol::kValueRequestAutoEnrollment;
    case DeviceManagementService::JobConfiguration::TYPE_REGISTRATION:
      return dm_protocol::kValueRequestRegister;
    case DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH:
      return dm_protocol::kValueRequestPolicy;
    case DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH:
      return dm_protocol::kValueRequestApiAuthorization;
    case DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION:
      return dm_protocol::kValueRequestUnregister;
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE:
      return dm_protocol::kValueRequestUploadCertificate;
    case DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL:
      return dm_protocol::kValueRequestDeviceStateRetrieval;
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS:
      return dm_protocol::kValueRequestUploadStatus;
    case DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS:
      return dm_protocol::kValueRequestRemoteCommands;
    case DeviceManagementService::JobConfiguration::
        TYPE_ATTRIBUTE_UPDATE_PERMISSION:
      return dm_protocol::kValueRequestDeviceAttributeUpdatePermission;
    case DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE:
      return dm_protocol::kValueRequestDeviceAttributeUpdate;
    case DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE:
      return dm_protocol::kValueRequestGcmIdUpdate;
    case DeviceManagementService::JobConfiguration::
        TYPE_ANDROID_MANAGEMENT_CHECK:
      return dm_protocol::kValueRequestCheckAndroidManagement;
    case DeviceManagementService::JobConfiguration::
        TYPE_CERT_BASED_REGISTRATION:
      return dm_protocol::kValueRequestCertBasedRegister;
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER:
      return dm_protocol::kValueRequestActiveDirectoryEnrollPlayUser;
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_PLAY_ACTIVITY:
      return dm_protocol::kValueRequestActiveDirectoryPlayActivity;
    case DeviceManagementService::JobConfiguration::TYPE_REQUEST_LICENSE_TYPES:
      return dm_protocol::kValueRequestCheckDeviceLicense;
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_APP_INSTALL_REPORT:
      return dm_protocol::kValueRequestAppInstallReport;
    case DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT:
      return dm_protocol::kValueRequestTokenEnrollment;
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT:
      return dm_protocol::kValueRequestChromeDesktopReport;
    case DeviceManagementService::JobConfiguration::
        TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL:
      return dm_protocol::kValueRequestInitialEnrollmentStateRetrieval;
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_POLICY_VALIDATION_REPORT:
      return dm_protocol::kValueRequestUploadPolicyValidationReport;
    case DeviceManagementService::JobConfiguration::TYPE_REQUEST_SAML_URL:
      return dm_protocol::kValueRequestPublicSamlUser;
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_REAL_TIME_REPORT:
      NOTREACHED() << "Not a DMServer request type" << type;
      break;
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT:
      return dm_protocol::kValueRequestChromeOsUserReport;
  }
  NOTREACHED() << "Invalid job type " << type;
  return "";
}

}  // namespace

DMServerJobConfiguration::DMServerJobConfiguration(
    DeviceManagementService* service,
    JobType type,
    const std::string& client_id,
    bool critical,
    std::unique_ptr<DMAuth> auth_data,
    base::Optional<std::string> oauth_token,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    Callback callback)
    : JobConfigurationBase(type, std::move(auth_data), oauth_token, factory),
      server_url_(service->configuration()->GetDMServerUrl()),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  AddParameter(dm_protocol::kParamRequest, JobTypeToRequestType(type));
  AddParameter(dm_protocol::kParamDeviceType, dm_protocol::kValueDeviceType);
  AddParameter(dm_protocol::kParamAppType, dm_protocol::kValueAppType);
  AddParameter(dm_protocol::kParamAgent,
               service->configuration()->GetAgentParameter());
  AddParameter(dm_protocol::kParamPlatform,
               service->configuration()->GetPlatformParameter());
  AddParameter(dm_protocol::kParamDeviceID, client_id);

  if (critical)
    AddParameter(dm_protocol::kParamCritical, "true");
}

DMServerJobConfiguration::DMServerJobConfiguration(
    JobType type,
    CloudPolicyClient* client,
    bool critical,
    std::unique_ptr<DMAuth> auth_data,
    base::Optional<std::string> oauth_token,
    Callback callback)
    : DMServerJobConfiguration(client->service(),
                               type,
                               client->client_id(),
                               critical,
                               std::move(auth_data),
                               oauth_token,
                               client->GetURLLoaderFactory(),
                               std::move(callback)) {}

DMServerJobConfiguration::~DMServerJobConfiguration() {}

DeviceManagementStatus
DMServerJobConfiguration::MapNetErrorAndResponseCodeToDMStatus(
    int net_error,
    int response_code) {
  DeviceManagementStatus code;
  if (net_error != net::OK) {
    code = DM_STATUS_REQUEST_FAILED;
  } else {
    switch (response_code) {
      case DeviceManagementService::kSuccess:
        code = DM_STATUS_SUCCESS;
        break;
      case DeviceManagementService::kInvalidArgument:
        code = DM_STATUS_REQUEST_INVALID;
        break;
      case DeviceManagementService::kInvalidAuthCookieOrDMToken:
        code = DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
        break;
      case DeviceManagementService::kMissingLicenses:
        code = DM_STATUS_SERVICE_MISSING_LICENSES;
        break;
      case DeviceManagementService::kDeviceManagementNotAllowed:
        code = DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED;
        break;
      case DeviceManagementService::kPendingApproval:
        code = DM_STATUS_SERVICE_ACTIVATION_PENDING;
        break;
      case DeviceManagementService::kRequestTooLarge:
        code = DM_STATUS_REQUEST_TOO_LARGE;
        break;
      case DeviceManagementService::kConsumerAccountWithPackagedLicense:
        code = DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE;
        break;
      case DeviceManagementService::kInvalidURL:
      case DeviceManagementService::kInternalServerError:
      case DeviceManagementService::kServiceUnavailable:
        code = DM_STATUS_TEMPORARY_UNAVAILABLE;
        break;
      case DeviceManagementService::kDeviceNotFound:
        code = DM_STATUS_SERVICE_DEVICE_NOT_FOUND;
        break;
      case DeviceManagementService::kPolicyNotFound:
        code = DM_STATUS_SERVICE_POLICY_NOT_FOUND;
        break;
      case DeviceManagementService::kInvalidSerialNumber:
        code = DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER;
        break;
      case DeviceManagementService::kDomainMismatch:
        code = DM_STATUS_SERVICE_DOMAIN_MISMATCH;
        break;
      case DeviceManagementService::kDeprovisioned:
        code = DM_STATUS_SERVICE_DEPROVISIONED;
        break;
      case DeviceManagementService::kDeviceIdConflict:
        code = DM_STATUS_SERVICE_DEVICE_ID_CONFLICT;
        break;
      case DeviceManagementService::kArcDisabled:
        code = DM_STATUS_SERVICE_ARC_DISABLED;
        break;
      default:
        // Handle all unknown 5xx HTTP error codes as temporary and any other
        // unknown error as one that needs more time to recover.
        if (response_code >= 500 && response_code <= 599)
          code = DM_STATUS_TEMPORARY_UNAVAILABLE;
        else
          code = DM_STATUS_HTTP_STATUS_ERROR;
        break;
    }
  }
  return code;
}

std::string DMServerJobConfiguration::GetPayload() {
  std::string payload;
  CHECK(request_.SerializeToString(&payload));
  return payload;
}

std::string DMServerJobConfiguration::GetUmaName() {
  return "Enterprise.DMServerRequestSuccess." + GetJobTypeAsString(GetType());
}

void DMServerJobConfiguration::OnURLLoadComplete(
    DeviceManagementService::Job* job,
    int net_error,
    int response_code,
    const std::string& response_body) {
  DeviceManagementStatus code =
      MapNetErrorAndResponseCodeToDMStatus(net_error, response_code);

  // Parse the response even if |response_code| is not a success since the
  // response data may contain an error message.
  em::DeviceManagementResponse response;
  if (code == DM_STATUS_SUCCESS &&
      (!response.ParseFromString(response_body) ||
       response_code != DeviceManagementService::kSuccess)) {
    code = DM_STATUS_RESPONSE_DECODING_ERROR;
    em::DeviceManagementResponse response;
    if (response.ParseFromString(response_body)) {
      LOG(WARNING) << "DMServer sent an error response: " << response_code
                   << ". " << response.error_message();
    } else {
      LOG(WARNING) << "DMServer sent an error response: " << response_code;
    }
  }

  std::move(callback_).Run(job, code, net_error, response);
}

GURL DMServerJobConfiguration::GetURL(int last_error) {
  // DM server requests always expect a dm_protocol::kParamRetry URL parameter
  // to indicate if this request is a retry.  Furthermore, if so then the
  // dm_protocol::kParamLastError URL parameter is also expected with the value
  // of the last error.

  GURL url(server_url_);
  url = net::AppendQueryParameter(url, dm_protocol::kParamRetry,
                                  last_error == 0 ? "false" : "true");

  if (last_error != 0) {
    url = net::AppendQueryParameter(url, dm_protocol::kParamLastError,
                                    base::NumberToString(last_error).c_str());
  }

  return url;
}

RegistrationJobConfiguration::RegistrationJobConfiguration(
    JobType type,
    CloudPolicyClient* client,
    std::unique_ptr<DMAuth> auth_data,
    base::Optional<std::string> oauth_token,
    Callback callback)
    : DMServerJobConfiguration(type,
                               client,
                               /*critical=*/false,
                               std::move(auth_data),
                               oauth_token,
                               std::move(callback)) {}

void RegistrationJobConfiguration::OnBeforeRetry() {
  // If the initial request managed to get to the server but the response
  // didn't arrive at the client then retrying with the same client ID will
  // fail. Set the re-registration flag so that the server accepts it.
  // If the server hasn't seen the client ID before then it will also accept
  // the re-registration.
  request()->mutable_register_request()->set_reregister(true);
}

}  // namespace policy
