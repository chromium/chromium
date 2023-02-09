// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

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
      NOTREACHED() << "Not a DMServer request type " << type;
      break;
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT:
      return dm_protocol::kValueRequestChromeOsUserReport;
    case DeviceManagementService::JobConfiguration::
        TYPE_CERT_PROVISIONING_REQUEST:
      return dm_protocol::kValueRequestCertProvisioningRequest;
    case DeviceManagementService::JobConfiguration::
        TYPE_PSM_HAS_DEVICE_STATE_REQUEST:
      return dm_protocol::kValueRequestPsmHasDeviceState;
    case DeviceManagementService::JobConfiguration::TYPE_CHECK_USER_ACCOUNT:
      return dm_protocol::kValueCheckUserAccount;
    case DeviceManagementService::JobConfiguration::
        TYPE_BROWSER_UPLOAD_PUBLIC_KEY:
      return dm_protocol::kValueBrowserUploadPublicKey;
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_ENCRYPTED_REPORT:
      NOTREACHED() << "Not a DMServer request type " << type;
      break;
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_EUICC_INFO:
      return dm_protocol::kValueRequestUploadEuiccInfo;
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_PROFILE_REPORT:
      return dm_protocol::kValueRequestChromeProfileReport;
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
    DMAuth auth_data,
    absl::optional<std::string> oauth_token,
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
    DMAuth auth_data,
    absl::optional<std::string> oauth_token,
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
DMServerJobConfiguration::MapNetErrorAndResponseToDMStatus(
    int net_error,
    int response_code,
    const std::string& response_body) {
  if (net_error != net::OK)
    return DM_STATUS_REQUEST_FAILED;

  switch (response_code) {
    case DeviceManagementService::kSuccess:
      return DM_STATUS_SUCCESS;
    case DeviceManagementService::kInvalidArgument:
      return DM_STATUS_REQUEST_INVALID;
    case DeviceManagementService::kInvalidAuthCookieOrDMToken:
      return DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
    case DeviceManagementService::kMissingLicenses:
      return DM_STATUS_SERVICE_MISSING_LICENSES;
    case DeviceManagementService::kDeviceManagementNotAllowed:
      return DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED;
    case DeviceManagementService::kPendingApproval:
      return DM_STATUS_SERVICE_ACTIVATION_PENDING;
    case DeviceManagementService::kRequestTooLarge:
      return DM_STATUS_REQUEST_TOO_LARGE;
    case DeviceManagementService::kConsumerAccountWithPackagedLicense:
      return DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE;
    case DeviceManagementService::kInvalidURL:
    case DeviceManagementService::kInternalServerError:
    case DeviceManagementService::kServiceUnavailable:
      return DM_STATUS_TEMPORARY_UNAVAILABLE;
    case DeviceManagementService::kDeviceNotFound: {
#if !BUILDFLAG(IS_CHROMEOS)
      if (!base::FeatureList::IsEnabled(features::kDmTokenDeletion))
        return DM_STATUS_SERVICE_DEVICE_NOT_FOUND;

      // If the DMToken deletion feature is enabled and forced, the DM status
      // corresponding to DMToken deletion will be returned regardless of
      // whether DMServer signals for deletion or invalidation. This is a
      // temporary feature intended for testing purposes only.
      if (base::GetFieldTrialParamByFeatureAsBool(features::kDmTokenDeletion,
                                                  /*param_name=*/"forced",
                                                  /*default_value=*/false))
        return DM_STATUS_SERVICE_DEVICE_NEEDS_RESET;

      // The `kDeviceNotFound` response code can correspond to different DM
      // statuses depending on the contents of the response body.
      em::DeviceManagementResponse response;
      if (response.ParseFromString(response_body) &&
          base::Contains(response.error_detail(),
                         em::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN)) {
        return DM_STATUS_SERVICE_DEVICE_NEEDS_RESET;
      }
#endif  // !BUILDFLAG(IS_CHROMEOS)
      return DM_STATUS_SERVICE_DEVICE_NOT_FOUND;
    }
    case DeviceManagementService::kPolicyNotFound:
      return DM_STATUS_SERVICE_POLICY_NOT_FOUND;
    case DeviceManagementService::kInvalidSerialNumber:
      return DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER;
    case DeviceManagementService::kTooManyRequests:
      return DM_STATUS_SERVICE_TOO_MANY_REQUESTS;
    case DeviceManagementService::kDomainMismatch:
      return DM_STATUS_SERVICE_DOMAIN_MISMATCH;
    case DeviceManagementService::kDeprovisioned:
      return DM_STATUS_SERVICE_DEPROVISIONED;
    case DeviceManagementService::kDeviceIdConflict:
      return DM_STATUS_SERVICE_DEVICE_ID_CONFLICT;
    case DeviceManagementService::kArcDisabled:
      return DM_STATUS_SERVICE_ARC_DISABLED;
    case DeviceManagementService::kInvalidDomainlessCustomer:
      return DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL;
    case DeviceManagementService::kTosHasNotBeenAccepted:
      return DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED;
    case DeviceManagementService::kIllegalAccountForPackagedEDULicense:
      return DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE;
    case DeviceManagementService::kInvalidPackagedDeviceForKiosk:
      return DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK;
    default:
      // Handle all unknown 5xx HTTP error codes as temporary and any other
      // unknown error as one that needs more time to recover.
      if (response_code >= 500 && response_code <= 599)
        return DM_STATUS_TEMPORARY_UNAVAILABLE;

      return DM_STATUS_HTTP_STATUS_ERROR;
  }
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
      MapNetErrorAndResponseToDMStatus(net_error, response_code, response_body);

  em::DeviceManagementResponse response;
  if (code == DM_STATUS_SUCCESS && !response.ParseFromString(response_body)) {
    code = DM_STATUS_RESPONSE_DECODING_ERROR;
    LOG_POLICY(WARNING, POLICY_FETCHING) << "DMServer sent an invalid response";
  } else if (response_code != DeviceManagementService::kSuccess) {
    if (response.ParseFromString(response_body)) {
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "DMServer sent an error response: " << response_code << ". "
          << response.error_message();
    } else {
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "DMServer sent an error response: " << response_code;
    }
  }

  std::move(callback_).Run(
      DMServerJobResult{job, net_error, code, std::move(response)});
}

GURL DMServerJobConfiguration::GetURL(int last_error) const {
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
    DMAuth auth_data,
    absl::optional<std::string> oauth_token,
    Callback callback)
    : DMServerJobConfiguration(type,
                               client,
                               /*critical=*/false,
                               std::move(auth_data),
                               oauth_token,
                               std::move(callback)) {}

void RegistrationJobConfiguration::SetTimeoutDuration(base::TimeDelta timeout) {
  timeout_ = timeout;
}

void RegistrationJobConfiguration::OnBeforeRetry(
    int response_code,
    const std::string& response_body) {
  // If the initial request managed to get to the server but the response
  // didn't arrive at the client then retrying with the same client ID will
  // fail. Set the re-registration flag so that the server accepts it.
  // If the server hasn't seen the client ID before then it will also accept
  // the re-registration.
  request()->mutable_register_request()->set_reregister(true);
}

}  // namespace policy
