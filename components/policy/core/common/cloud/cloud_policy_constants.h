// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
POLICY_EXPORT extern const char kParamAgent[];
POLICY_EXPORT extern const char kParamAppType[];
POLICY_EXPORT extern const char kParamCritical[];
POLICY_EXPORT extern const char kParamDeviceID[];
POLICY_EXPORT extern const char kParamDeviceType[];
POLICY_EXPORT extern const char kParamLastError[];
POLICY_EXPORT extern const char kParamOAuthToken[];
POLICY_EXPORT extern const char kParamPlatform[];
POLICY_EXPORT extern const char kParamRequest[];
POLICY_EXPORT extern const char kParamRetry[];

// Policy constants used in authorization header.
POLICY_EXPORT extern const char kAuthHeader[];
POLICY_EXPORT extern const char kServiceTokenAuthHeaderPrefix[];
POLICY_EXPORT extern const char kDMTokenAuthHeaderPrefix[];
POLICY_EXPORT extern const char kEnrollmentTokenAuthHeaderPrefix[];

// String extern constants for the device and app type we report to the server.
POLICY_EXPORT extern const char kValueAppType[];
POLICY_EXPORT extern const char kValueDeviceType[];
POLICY_EXPORT extern const char kValueRequestAutoEnrollment[];
POLICY_EXPORT extern const char kValueRequestPolicy[];
POLICY_EXPORT extern const char kValueRequestRegister[];
POLICY_EXPORT extern const char kValueRequestApiAuthorization[];
POLICY_EXPORT extern const char kValueRequestUnregister[];
POLICY_EXPORT extern const char kValueRequestUploadCertificate[];
POLICY_EXPORT extern const char kValueRequestDeviceStateRetrieval[];
POLICY_EXPORT extern const char kValueRequestUploadStatus[];
POLICY_EXPORT extern const char kValueRequestRemoteCommands[];
POLICY_EXPORT extern const char kValueRequestDeviceAttributeUpdatePermission[];
POLICY_EXPORT extern const char kValueRequestDeviceAttributeUpdate[];
POLICY_EXPORT extern const char kValueRequestGcmIdUpdate[];
POLICY_EXPORT extern const char kValueRequestCheckAndroidManagement[];
POLICY_EXPORT extern const char kValueRequestCertBasedRegister[];
POLICY_EXPORT extern const char kValueRequestActiveDirectoryEnrollPlayUser[];
POLICY_EXPORT extern const char kValueRequestActiveDirectoryPlayActivity[];
POLICY_EXPORT extern const char kValueRequestCheckDeviceLicense[];
POLICY_EXPORT extern const char kValueRequestAppInstallReport[];
POLICY_EXPORT extern const char kValueRequestTokenEnrollment[];
POLICY_EXPORT extern const char kValueRequestChromeDesktopReport[];
POLICY_EXPORT extern const char kValueRequestInitialEnrollmentStateRetrieval[];
POLICY_EXPORT extern const char kValueRequestUploadPolicyValidationReport[];
POLICY_EXPORT extern const char kValueRequestPublicSamlUser[];
POLICY_EXPORT extern const char kValueRequestChromeOsUserReport[];

// Policy type strings for the policy_type field in PolicyFetchRequest.
POLICY_EXPORT extern const char kChromeDevicePolicyType[];
POLICY_EXPORT extern const char kChromeUserPolicyType[];
POLICY_EXPORT extern const char kChromePublicAccountPolicyType[];
POLICY_EXPORT extern const char kChromeExtensionPolicyType[];
POLICY_EXPORT extern const char kChromeSigninExtensionPolicyType[];
POLICY_EXPORT extern const char kChromeMachineLevelUserCloudPolicyType[];
POLICY_EXPORT extern const char kChromeMachineLevelExtensionCloudPolicyType[];
POLICY_EXPORT extern const char kChromeRemoteCommandPolicyType[];

// These codes are sent in the |error_code| field of PolicyFetchResponse.
enum PolicyFetchStatus {
  POLICY_FETCH_SUCCESS = 200,
  POLICY_FETCH_ERROR_NOT_FOUND = 902,
};

}  // namespace dm_protocol

// Public half of the verification key that is used to verify that policy
// signing keys are originating from DM server.
POLICY_EXPORT std::string GetPolicyVerificationKey();

// Corresponding hash.
POLICY_EXPORT extern const char kPolicyVerificationKeyHash[];

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
  // Service error: Policy not found. Error code defined by the DM folks.
  DM_STATUS_SERVICE_POLICY_NOT_FOUND = 902,
  // Service error: ARC is not enabled on this domain.
  DM_STATUS_SERVICE_ARC_DISABLED = 904,
  // Service error: Non-dasher account with packaged license can't enroll.
  DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE = 905,
};

// List of modes that the device can be locked into.
enum DeviceMode {
  DEVICE_MODE_PENDING,             // The device mode is not yet available.
  DEVICE_MODE_NOT_SET,             // The device is not yet enrolled or owned.
  DEVICE_MODE_CONSUMER,            // The device is locally owned as consumer
                                   // device.
  DEVICE_MODE_ENTERPRISE,          // The device is enrolled as an enterprise
                                   // device.
  DEVICE_MODE_ENTERPRISE_AD,       // The device has joined AD.
  DEVICE_MODE_LEGACY_RETAIL_MODE,  // The device is enrolled as a retail kiosk
                                   // device. Even though retail mode is
                                   // deprecated, we still check for this device
                                   // mode so that if an existing device is
                                   // still enrolled in retail mode, we take the
                                   // appropriate action (currently, launching
                                   // offline demo mode).
  DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,  // The device is locally owned as
                                          // consumer kiosk with ability to auto
                                          // launch a kiosk webapp.
  DEVICE_MODE_DEMO,                       // The device is in demo mode. It was
                                          // either enrolled online or setup
                                          // offline into demo mode domain -
                                          // see kDemoModeDomain.
};

// Domain that demo mode devices are enrolled into: cros-demo-mode.com
POLICY_EXPORT extern const char kDemoModeDomain[];

// License types available for enrollment.
enum class LicenseType {
  UNKNOWN,    // Included for compatibility.
  PERPETUAL,  // Perpetual license
  ANNUAL,     // Annual license
  KIOSK       // Single App Kiosk license
};

// Indicate this device's market segment. go/cros-rlz-segments
enum class MarketSegment {
  UNKNOWN,  // If device is not enrolled or market segment is not specified.
  EDUCATION,
  ENTERPRISE,
};

// Sender ID of FCM (Firebase Cloud Messaging)
// Policy Invalidation sender coming from the Firebase console.
extern const char kPolicyFCMInvalidationSenderID[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_
