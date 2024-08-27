// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_

#include <string>

#include "components/policy/policy_export.h"

namespace policy {

// Constants related to the device management protocol.
namespace dm_protocol {

// Name extern constants for URL query parameters.
extern const char kParamAgent[];
extern const char kParamAppType[];
extern const char kParamCritical[];
extern const char kParamDeviceID[];
extern const char kParamDeviceType[];
extern const char kParamLastError[];
extern const char kParamOAuthToken[];
extern const char kParamPlatform[];
extern const char kParamRequest[];
extern const char kParamRetry[];
extern const char kParamProfileID[];

// Policy constants used in authorization header.
extern const char kAuthHeader[];
extern const char kServiceTokenAuthHeaderPrefix[];
extern const char kDMTokenAuthHeaderPrefix[];
extern const char kEnrollmentTokenAuthHeaderPrefix[];
extern const char kOAuthTokenHeaderPrefix[];
extern const char kOidcAuthHeaderPrefix[];
extern const char kOidcAuthTokenHeaderPrefix[];
extern const char kOidcIdTokenHeaderPrefix[];

// String extern constants for the device and app type we report to the server.
extern const char kValueAppType[];
extern const char kValueBrowserUploadPublicKey[];
extern const char kValueDeviceType[];
extern const char kValueRequestAutoEnrollment[];
extern const char kValueRequestPsmHasDeviceState[];
extern const char kValueCheckUserAccount[];
extern const char kValueRequestPolicy[];
extern const char kValueRequestRegister[];
extern const char kValueRequestRegisterProfile[];
extern const char kValueRequestApiAuthorization[];
extern const char kValueRequestUnregister[];
extern const char kValueRequestUploadCertificate[];
extern const char kValueRequestUploadEuiccInfo[];
extern const char kValueRequestDeviceStateRetrieval[];
extern const char kValueRequestUploadStatus[];
extern const char kValueRequestRemoteCommands[];
extern const char kValueRequestDeviceAttributeUpdatePermission[];
extern const char kValueRequestDeviceAttributeUpdate[];
extern const char kValueRequestGcmIdUpdate[];
extern const char kValueRequestCheckAndroidManagement[];
extern const char kValueRequestCertBasedRegister[];
extern const char kValueRequestTokenBasedRegister[];
extern const char kValueRequestActiveDirectoryEnrollPlayUser[];
extern const char kValueRequestActiveDirectoryPlayActivity[];
extern const char kValueRequestAppInstallReport[];
extern const char kValueRequestRegisterBrowser[];
extern const char kValueRequestRegisterPolicyAgent[];
extern const char kValueRequestChromeDesktopReport[];
extern const char kValueRequestInitialEnrollmentStateRetrieval[];
extern const char kValueRequestUploadPolicyValidationReport[];
extern const char kValueRequestPublicSamlUser[];
extern const char kValueRequestChromeOsUserReport[];
extern const char kValueRequestCertProvisioningRequest[];
extern const char kValueRequestChromeProfileReport[];
extern const char kValueRequestFmRegistrationTokenUpload[];

// Policy type strings for the policy_type field in PolicyFetchRequest.
extern const char kChromeDevicePolicyType[];
extern const char kChromeUserPolicyType[];
extern const char kChromePublicAccountPolicyType[];
extern const char kChromeExtensionPolicyType[];
extern const char kChromeSigninExtensionPolicyType[];
extern const char kChromeMachineLevelUserCloudPolicyType[];
extern const char kChromeMachineLevelExtensionCloudPolicyType[];
extern const char kChromeRemoteCommandPolicyType[];
extern const char kGoogleUpdateMachineLevelAppsPolicyType[];
extern const char kGoogleUpdateMachineLevelOmahaPolicyType[];

// Remote command type for `type` field in DeviceRemoteCommandRequest.
// Command for Chrome OS Ash user.
extern const char kChromeAshUserRemoteCommandType[];
// Command for Chrome OS device.
extern const char kChromeDeviceRemoteCommandType[];
// Command for CBCM device on non-CrOS
extern const char kChromeBrowserRemoteCommandType[];
// Command for browser profile.
extern const char kChromeUserRemoteCommandType[];

extern const char kChromeMachineLevelUserCloudPolicyTypeBase64[];

// These codes are sent in the |error_code| field of PolicyFetchResponse.
enum PolicyFetchStatus {
  POLICY_FETCH_SUCCESS = 200,
  POLICY_FETCH_ERROR_NOT_FOUND = 902,
};

}  // namespace dm_protocol

// Public half of the verification key that is used to verify that policy
// signing keys are originating from DM server.
std::string GetPolicyVerificationKey();

// Corresponding hash.
extern const char kPolicyVerificationKeyHash[];

// Status codes for communication errors with the device management service.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DeviceManagementStatus {
  // All is good.
  DM_STATUS_SUCCESS = 0,
  // Request payload invalid.
  DM_STATUS_REQUEST_INVALID = 1,
  // The HTTP request failed.
  DM_STATUS_REQUEST_FAILED = 2,
  // The server returned an error code that points to a temporary problem.
  DM_STATUS_TEMPORARY_UNAVAILABLE = 3,
  // The HTTP request returned a non-success code.
  DM_STATUS_HTTP_STATUS_ERROR = 4,
  // Response could not be decoded.
  DM_STATUS_RESPONSE_DECODING_ERROR = 5,
  // Service error: Management not supported.
  DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED = 6,
  // Service error: Device not found.
  DM_STATUS_SERVICE_DEVICE_NOT_FOUND = 7,
  // Service error: Device token invalid.
  DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID = 8,
  // Service error: Activation pending.
  DM_STATUS_SERVICE_ACTIVATION_PENDING = 9,
  // Service error: The serial number is not valid or not known to the server.
  DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER = 10,
  // Service error: The device id used for registration is already taken.
  DM_STATUS_SERVICE_DEVICE_ID_CONFLICT = 11,
  // Service error: The licenses have expired or have been exhausted.
  DM_STATUS_SERVICE_MISSING_LICENSES = 12,
  // Service error: The administrator has deprovisioned this client.
  DM_STATUS_SERVICE_DEPROVISIONED = 13,
  // Service error: Device registration for the wrong domain.
  DM_STATUS_SERVICE_DOMAIN_MISMATCH = 14,
  // Client error: Request could not be signed.
  DM_STATUS_CANNOT_SIGN_REQUEST = 15,
  // Client error: Request body is too large.
  DM_STATUS_REQUEST_TOO_LARGE = 16,
  // Client error: Too many request.
  DM_STATUS_SERVICE_TOO_MANY_REQUESTS = 17,
  // Service error: The device needs to be reset (ex. for re-enrollment).
  DM_STATUS_SERVICE_DEVICE_NEEDS_RESET = 18,
  // Service error: Policy not found. Error code defined by the DM folks.
  DM_STATUS_SERVICE_POLICY_NOT_FOUND = 902,
  // Service error: ARC is not enabled on this domain.
  DM_STATUS_SERVICE_ARC_DISABLED = 904,
  // Service error: Non-dasher account with packaged license can't enroll.
  DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE = 905,
  // Service error: Not eligible enterprise account can't enroll.
  DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL = 906,
  // Service error: Enterprise TOS has not been accepted.
  DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED = 907,
  // Service error: Illegal account for packaged EDU license.
  DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE = 908,
  // Service error: Packaged license device can't enroll KIOSK.
  DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK = 909
};

// List of modes that the device can be locked into. Some IDs are skipped
// because they have been used in the past but got deprecated and deleted.
enum DeviceMode {
  DEVICE_MODE_PENDING = 0,     // The device mode is not yet available.
  DEVICE_MODE_NOT_SET = 1,     // The device is not yet enrolled or owned.
  DEVICE_MODE_CONSUMER = 2,    // The device is locally owned as consumer
                               // device.
  DEVICE_MODE_ENTERPRISE = 3,  // The device is enrolled as an enterprise
                               // device.
  DEPRECATED_DEVICE_MODE_LEGACY_RETAIL_MODE = 5,  // The device is enrolled as a
                                                  // retail kiosk device. This
                                                  // is deprecated.
  DEPRECATED_DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH = 6,  // The device is
                                                         // locally owned as
                                                         // consumer kiosk with
                                                         // ability to auto
                                                         // launch a kiosk
                                                         // webapp. This is
                                                         // deprecated.
  DEVICE_MODE_DEMO = 7,  // The device is in demo mode. It was
                         // either enrolled online or setup
                         // offline into demo mode domain -
                         // see kDemoModeDomain.
};

// List of modes of OIDC management.
enum ThirdPartyIdentityType {
  NO_THIRD_PARTY_MANAGEMENT =
      0,  // The device mode is not managed by a third party identity.
  OIDC_MANAGEMENT_DASHER_BASED =
      1,  // The device mode is managed by a third party identity that is
          // sync-ed to Google.
  OIDC_MANAGEMENT_DASHERLESS =
      2,  // The device mode is managed by a third party identity that is
          // notsync-ed to Google.
};

// Domain that demo mode devices are enrolled into: cros-demo-mode.com
extern const char kDemoModeDomain[];

// Indicate this device's market segment. go/cros-rlz-segments.
// This enum should be kept in sync with MarketSegment enum in
// device_management_backend.proto (http://shortn/_p0P58C4BRV). If any additions
// are made to this proto, the UserDeviceMatrix in
// src/tools/metrics/histograms/enums.xml should also be updated, as well as the
// browser test suite in usertype_by_devicetype_metrics_provider_browsertest.cc
// (http://shortn/_gD5uIM9Z78) to account for the new user / device type combo.
enum class MarketSegment {
  UNKNOWN,  // If device is not enrolled or market segment is not specified.
  EDUCATION,
  ENTERPRISE,
};

// Sender ID of FCM (Firebase Cloud Messaging)
// Policy Invalidation sender coming from the Firebase console.
inline constexpr char kPolicyFCMInvalidationSenderID[] = "1013309121859";

// Kiosk SKU name. This is the constant of the enrollment license type that
// exists on the server side.
inline static const char kKioskSkuName[] = "GOOGLE.CHROME_KIOSK_ANNUAL";

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_
