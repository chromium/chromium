// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_

#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {

// Metrics collected for enterprise events.

// Events related to device enrollment.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum MetricEnrollment {
  // User pressed 'Cancel' during the enrollment process.
  kMetricEnrollmentCancelled = 0,
  // User started enrollment process by submitting valid credentials.
  kMetricEnrollmentStarted = 1,
  // OAuth token fetch failed: network error.
  kMetricEnrollmentNetworkFailed = 2,
  // OAuth token fetch failed: login error.
  kMetricEnrollmentLoginFailed = 3,
  // Registration / policy fetch failed: DM server reports management not
  // supported.
  kMetricEnrollmentNotSupported = 4,
  /* kMetricEnrollmentPolicyFailed = 5 REMOVED */
  /* kMetricEnrollmentOtherFailed = 6 REMOVED */
  // Enrollment was successful.
  kMetricEnrollmentOK = 7,
  // Registration / policy fetch failed: DM server reports that the serial
  // number we try to register is not assigned to the domain used.
  kMetricEnrollmentRegisterPolicyInvalidSerial = 8,
  /* kMetricEnrollmentAutoStarted = 9 REMOVED */
  /* kMetricEnrollmentAutoFailed = 10 REMOVED */
  /* kMetricEnrollmentAutoRestarted = 11 REMOVED */
  /* kMetricEnrollmentAutoCancelled = 12 REMOVED */
  /* kMetricEnrollmentAutoOK = 13 REMOVED */
  // Registration failed: DM server returns unknown/disallowed enrollment mode.
  kMetricEnrollmentInvalidEnrollmentMode = 14,
  /* kMetricEnrollmentAutoEnrollmentNotSupported = 15 REMOVED */
  // Lockbox initialization took too long to complete.
  kMetricEnrollmentLockboxTimeoutError = 16,
  // Lockbox error at re-enrollment: domain does not match install attributes.
  kMetricEnrollmentLockDomainMismatch = 17,
  // Registration / policy fetch failed: DM server reports licenses expired or
  // exhausted.
  kMetricEnrollmentRegisterPolicyMissingLicenses = 18,
  // Failed to fetch device robot authorization code from DM Server.
  kMetricEnrollmentRobotAuthCodeFetchFailed = 19,
  // Failed to fetch device robot refresh token from GAIA.
  kMetricEnrollmentRobotRefreshTokenFetchFailed = 20,
  // Failed to persist robot account refresh token on device.
  kMetricEnrollmentRobotRefreshTokenStoreFailed = 21,
  // Registration / policy fetch failed: DM server reports administrator
  // deprovisioned the device.
  kMetricEnrollmentRegisterPolicyDeprovisioned = 22,
  // Registration / policy fetch failed: DM server reports domain mismatch.
  kMetricEnrollmentRegisterPolicyDomainMismatch = 23,
  // Enrollment has been triggered, the webui login screen has been shown.
  kMetricEnrollmentTriggered = 24,
  // The user submitted valid credentials to start the enrollment process
  // for the second (or further) time.
  kMetricEnrollmentRestarted = 25,
  /* kMetricEnrollmentStoreTokenAndIdFailed = 26 REMOVED */
  // Failed to obtain FRE state keys.
  kMetricEnrollmentNoStateKeys = 27,
  // Failed to validate policy.
  kMetricEnrollmentPolicyValidationFailed = 28,
  // Failed due to error in CloudPolicyStore.
  kMetricEnrollmentCloudPolicyStoreError = 29,
  /* kMetricEnrollmentLockBackendError = 30 REMOVED */
  // Registration / policy fetch failed: DM server reports invalid request
  // payload.
  kMetricEnrollmentRegisterPolicyPayloadInvalid = 31,
  // Registration / policy fetch failed: DM server reports device not found.
  kMetricEnrollmentRegisterPolicyDeviceNotFound = 32,
  // Registration / policy fetch failed: DM server reports DM token invalid.
  kMetricEnrollmentRegisterPolicyDMTokenInvalid = 33,
  // Registration / policy fetch failed: DM server reports activation pending.
  kMetricEnrollmentRegisterPolicyActivationPending = 34,
  // Registration / policy fetch failed: DM server reports device ID conflict.
  kMetricEnrollmentRegisterPolicyDeviceIdConflict = 35,
  // Registration / policy fetch failed: DM server can't find policy.
  kMetricEnrollmentRegisterPolicyNotFound = 36,
  // Registration / policy fetch failed: HTTP request failed.
  kMetricEnrollmentRegisterPolicyRequestFailed = 37,
  // Registration / policy fetch failed: DM server reports temporary problem.
  kMetricEnrollmentRegisterPolicyTempUnavailable = 38,
  // Registration / policy fetch failed: DM server returns non-success HTTP
  // status code.
  kMetricEnrollmentRegisterPolicyHttpError = 39,
  // Registration / policy fetch failed: can't decode DM server response.
  kMetricEnrollmentRegisterPolicyResponseInvalid = 40,
  // OAuth token fetch failed: account not signed up.
  kMetricEnrollmentAccountNotSignedUp = 41,
  /* kMetricEnrollmentAccountDeleted = 42 REMOVED */
  /* kMetricEnrollmentAccountDisabled = 43 REMOVED */
  // Re-enrollment pre-check failed: domain does not match install attributes.
  kMetricEnrollmentPrecheckDomainMismatch = 44,
  // Lockbox backend failed to initialize.
  kMetricEnrollmentLockBackendInvalid = 45,
  // Lockbox backend (TPM) already locked.
  kMetricEnrollmentLockAlreadyLocked = 46,
  // Lockbox failure setting attributes.
  kMetricEnrollmentLockSetError = 47,
  // Lockbox failure during locking.
  kMetricEnrollmentLockFinalizeError = 48,
  // Lockbox read back is inconsistent.
  kMetricEnrollmentLockReadbackError = 49,
  // Failed to update device attributes.
  kMetricEnrollmentAttributeUpdateFailed = 50,
  // Enrollment mode does not match already locked install attributes.
  kMetricEnrollmentLockModeMismatch = 51,
  // A registration certificate could not be fetched from the PCA.
  kMetricEnrollmentRegistrationCertificateFetchFailed = 52,
  // The request to enroll could not be signed.
  kMetricEnrollmentRegisterCannotSignRequest = 53,
  // Device model or serial number missing from VPD.
  kMetricEnrollmentNoDeviceIdentification = 54,
  // Active Directory policy fetch failed.
  kMetricEnrollmentActiveDirectoryPolicyFetchFailed = 55,
  // Failed to store DM token into the local state.
  kMetricEnrollmentStoreDMTokenFailed = 56,
  // Failed to get available licenses.
  kMetricEnrollmentLicenseRequestFailed = 57,
  // Registration failed: Consumer account with packaged license.
  kMetricEnrollmentRegisterConsumerAccountWithPackagedLicense = 58,
  // Device was not pre-provisioned for Zero-Touch.
  kMetricEnrollmentDeviceNotPreProvisioned = 59,
  // Enrollment failed: Enterprise account is not eligible to enroll.
  kMetricEnrollmentRegisterEnterpriseAccountIsNotEligibleToEnroll = 60,
  // Enrollment failed: Enterprise TOS has not been accepted.
  kMetricEnrollmentRegisterEnterpriseTosHasNotBeenAccepted = 61,
  // Too many requests are uploadede within a short time.
  kMetricEnrollmentTooManyRequests = 62,
  // Enrollment failed: illegal account for packaged EDU license.
  kMetricEnrollmentIllegalAccountForPackagedEDULicense = 63,
  // Enrollment failed: dev mode would be blocked but this is prevented by a
  // command-line switch.
  kMetricEnrollmentMayNotBlockDevMode = 64,
  // Enrollment failed: Packaged license device invalid for KIOSK.
  kMetricEnrollmentInvalidPackagedDeviceForKIOSK = 65,
  // Max value for use with enumeration histogram UMA functions.
  kMaxValue = kMetricEnrollmentInvalidPackagedDeviceForKIOSK
};

// Events related to policy refresh.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum MetricPolicyRefresh {
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were enabled.
  METRIC_POLICY_REFRESH_CHANGED = 0,
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were disabled.
  METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS = 1,
  // A refresh occurred while the policy was not invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_UNCHANGED = 2,
  // A refresh occurred while the policy was invalidated and the policy was
  // changed.
  METRIC_POLICY_REFRESH_INVALIDATED_CHANGED = 3,
  // A refresh occurred while the policy was invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED = 4,

  METRIC_POLICY_REFRESH_SIZE  // Must be the last.
};

// Types of policy invalidations.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum PolicyInvalidationType {
  // The invalidation contained no payload.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD = 0,
  // A normal invalidation containing a payload.
  POLICY_INVALIDATION_TYPE_NORMAL = 1,
  // The invalidation contained no payload and was considered expired.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED = 3,
  // The invalidation contained a payload and was considered expired.
  POLICY_INVALIDATION_TYPE_EXPIRED = 4,

  POLICY_INVALIDATION_TYPE_SIZE  // Must be the last.
};

// Result of the Device ID field validation in policy protobufs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PolicyDeviceIdValidity {
  kValid = 0,
  kActualIdUnknown = 1,
  kMissing = 2,
  kInvalid = 3,
  kMaxValue = kInvalid,  // Must be the last.
};

// Names for the UMA counters. They are shared from here since the events
// from the same enum above can be triggered in different files, and must use
// the same UMA histogram name.
// Metrics name from UMA dashboard cloud be used in codesearch as is, so please
// keep the names without format specifiers (e.g. %s) or add a comment how the
// name could be expanded.
POLICY_EXPORT extern const char kMetricUserPolicyRefresh[];
POLICY_EXPORT extern const char kMetricUserPolicyRefreshFcm[];
POLICY_EXPORT extern const char kMetricUserPolicyInvalidations[];
POLICY_EXPORT extern const char kMetricUserPolicyInvalidationsFcm[];

POLICY_EXPORT extern const char kMetricDevicePolicyRefresh[];
POLICY_EXPORT extern const char kMetricDevicePolicyRefreshFcm[];
POLICY_EXPORT extern const char kMetricDevicePolicyInvalidations[];
POLICY_EXPORT extern const char kMetricDevicePolicyInvalidationsFcm[];

POLICY_EXPORT extern const char kMetricDeviceLocalAccountPolicyRefresh[];
POLICY_EXPORT extern const char kMetricDeviceLocalAccountPolicyRefreshFcm[];
POLICY_EXPORT extern const char kMetricDeviceLocalAccountPolicyInvalidations[];
POLICY_EXPORT extern const char
    kMetricDeviceLocalAccountPolicyInvalidationsFcm[];

POLICY_EXPORT extern const char kMetricCBCMPolicyRefresh[];
POLICY_EXPORT extern const char kMetricCBCMPolicyRefreshFcm[];
POLICY_EXPORT extern const char kMetricCBCMPolicyInvalidations[];
POLICY_EXPORT extern const char kMetricCBCMPolicyInvalidationsFcm[];

POLICY_EXPORT extern const char kMetricPolicyInvalidationRegistration[];
POLICY_EXPORT extern const char kMetricPolicyInvalidationRegistrationFcm[];

POLICY_EXPORT extern const char kMetricUserRemoteCommandInvalidations[];
POLICY_EXPORT extern const char kMetricDeviceRemoteCommandInvalidations[];
POLICY_EXPORT extern const char kMetricCBCMRemoteCommandInvalidations[];

POLICY_EXPORT extern const char
    kMetricRemoteCommandInvalidationsRegistrationResult[];

POLICY_EXPORT extern const char kMetricUserRemoteCommandReceived[];
POLICY_EXPORT extern const char kMetricUserRemoteCommandExecutedTemplate[];

POLICY_EXPORT extern const char kMetricDeviceRemoteCommandReceived[];
POLICY_EXPORT extern const char kMetricDeviceRemoteCommandExecutedTemplate[];

POLICY_EXPORT extern const char kMetricCBCMRemoteCommandReceived[];
POLICY_EXPORT extern const char kMetricCBCMRemoteCommandExecutedTemplate[];

// Private set membership UMA histogram names.
POLICY_EXPORT extern const char kUMAPsmSuccessTime[];
POLICY_EXPORT extern const char kUMAPsmResult[];
POLICY_EXPORT extern const char kUMAPsmNetworkErrorCode[];
POLICY_EXPORT extern const char kUMAPsmDmServerRequestStatus[];

// DeviceAutoEnrollmentRequest i.e. hash dance request UMA histogram names.
POLICY_EXPORT extern const char kUMAHashDanceSuccessTime[];
// The following histogram names where added before PSM (private set membership)
// existed. They are only recorded for hash dance.
POLICY_EXPORT extern const char kUMAHashDanceProtocolTime[];
POLICY_EXPORT extern const char kUMAHashDanceBucketDownloadTime[];
POLICY_EXPORT extern const char kUMAHashDanceExtraTime[];
POLICY_EXPORT extern const char kUMAHashDanceRequestStatus[];
POLICY_EXPORT extern const char kUMAHashDanceNetworkErrorCode[];

// The following UMA suffixes are used by Hash dance and PSM protocols.
// Suffix for initial enrollment.
POLICY_EXPORT extern const char kUMASuffixInitialEnrollment[];
// Suffix for Forced Re-Enrollment.
POLICY_EXPORT extern const char kUMASuffixFRE[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
