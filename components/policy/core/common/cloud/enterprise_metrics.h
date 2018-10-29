// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_

#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {

// Metrics collected for enterprise events.

// Events related to fetching, saving and loading DM server tokens.
// These metrics are collected both for device and user tokens.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum MetricToken {
  // A cached token was successfully loaded from disk.
  kMetricTokenLoadSucceeded = 0,
  // Reading a cached token from disk failed.
  kMetricTokenLoadFailed = 1,

  // A token fetch request was sent to the DM server.
  kMetricTokenFetchRequested = 2,
  // The request was invalid, or the HTTP request failed.
  kMetricTokenFetchRequestFailed = 3,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricTokenFetchServerFailed = 4,
  // A response to the fetch request was received.
  kMetricTokenFetchResponseReceived = 5,
  // The response received was invalid. This happens when some expected data
  // was not present in the response.
  kMetricTokenFetchBadResponse = 6,
  // DM server reported that management is not supported.
  kMetricTokenFetchManagementNotSupported = 7,
  // DM server reported that the given device ID was not found.
  kMetricTokenFetchDeviceNotFound = 8,
  // DM token successfully retrieved.
  kMetricTokenFetchOK = 9,

  // Successfully cached a token to disk.
  kMetricTokenStoreSucceeded = 10,
  // Caching a token to disk failed.
  kMetricTokenStoreFailed = 11,

  // DM server reported that the device-id generated is not unique.
  kMetricTokenFetchDeviceIdConflict = 12,
  // DM server reported that the serial number we try to register is invalid.
  kMetricTokenFetchInvalidSerialNumber = 13,
  // DM server reported that the licenses for the domain have expired or been
  // exhausted.
  kMetricMissingLicenses = 14,

  kMetricTokenSize  // Must be the last.
};

// Events related to fetching, saving and loading user and device policies.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum MetricPolicy {
  // A cached policy was successfully loaded from disk.
  kMetricPolicyLoadSucceeded = 0,
  // Reading a cached policy from disk failed.
  kMetricPolicyLoadFailed = 1,

  // A policy fetch request was sent to the DM server.
  kMetricPolicyFetchRequested = 2,
  // The request was invalid, or the HTTP request failed.
  kMetricPolicyFetchRequestFailed = 3,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricPolicyFetchServerFailed = 4,
  // Policy not found for the given user or device.
  kMetricPolicyFetchNotFound = 5,
  // DM server didn't accept the token used in the request.
  kMetricPolicyFetchInvalidToken = 6,
  // A response to the policy fetch request was received.
  kMetricPolicyFetchResponseReceived = 7,
  // The policy response message didn't contain a policy, or other data was
  // missing.
  kMetricPolicyFetchBadResponse = 8,
  // Failed to decode the policy.
  kMetricPolicyFetchInvalidPolicy = 9,
  // The device policy was rejected because its signature was invalid.
  kMetricPolicyFetchBadSignature = 10,
  // Rejected policy because its timestamp is in the future.
  kMetricPolicyFetchTimestampInFuture = 11,
  // Device policy rejected because the device is not managed.
  kMetricPolicyFetchNonEnterpriseDevice = 12,
  // The policy was provided for a username that is different from the device
  // owner, and the policy was rejected.
  kMetricPolicyFetchUserMismatch = 13,
  // The policy was rejected for another reason. Currently this can happen
  // only for device policies, when the SignedSettings fail to store or retrieve
  // a stored policy.
  kMetricPolicyFetchOtherFailed = 14,
  // The fetched policy was accepted.
  kMetricPolicyFetchOK = 15,
  // The policy just fetched didn't have any changes compared to the cached
  // policy.
  kMetricPolicyFetchNotModified = 16,

  // Successfully cached a policy to disk.
  kMetricPolicyStoreSucceeded = 17,
  // Caching a policy to disk failed.
  kMetricPolicyStoreFailed = 18,

  kMetricPolicySize  // Must be the last.
};

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
  // OAuth token fetch failed: account deleted.
  kMetricEnrollmentAccountDeleted = 42,
  // OAuth token fetch failed: account disabled.
  kMetricEnrollmentAccountDisabled = 43,
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

#if defined(OS_CHROMEOS)
// Events related to Chrome OS user policy which cause session abort.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration
//       (update tools/metrics/histograms/enums.xml as well).
enum class MetricUserPolicyChromeOSSessionAbortType {
  // Abort of asynchronous user policy initialization when the user is managed
  // with the Google cloud management.
  kInitWithGoogleCloudManagement = 0,
  // Abort of asynchronous user policy initialization when the user is managed
  // with the Active Directory management.
  kInitWithActiveDirectoryManagement = 1,
  // Abort of blocking (synchronous) user policy initialization when the user is
  // managed with the Google cloud management.
  kBlockingInitWithGoogleCloudManagement = 2,
  // Abort of blocking (synchronous) user policy initialization when the user is
  // managed with the Active Directory management.
  kBlockingInitWithActiveDirectoryManagement = 3,

  kCount,  // Must be the last.
};
#endif  // defined(OS_CHROMEOS)

// Names for the UMA counters. They are shared from here since the events
// from the same enum above can be triggered in different files, and must use
// the same UMA histogram name.
POLICY_EXPORT extern const char kMetricToken[];
POLICY_EXPORT extern const char kMetricPolicy[];
POLICY_EXPORT extern const char kMetricUserPolicyRefresh[];
POLICY_EXPORT extern const char kMetricUserPolicyInvalidations[];
POLICY_EXPORT extern const char kMetricUserPolicyChromeOSSessionAbort[];
POLICY_EXPORT extern const char kMetricDevicePolicyRefresh[];
POLICY_EXPORT extern const char kMetricDevicePolicyInvalidations[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
