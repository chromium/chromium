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
// This enum is used to define the buckets for an enumerated UMA histogram with
// the name of EnterpriseEnrollmentType.
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
  // A registration certificate could not be fetched from the PCA due to
  // unspecified failure.
  kMetricEnrollmentRegistrationCertificateFetchUnspecifiedFailure = 66,
  // A registration certificate could not be fetched from the PCA due to
  // bad request.
  kMetricEnrollmentRegistrationCertificateFetchBadRequest = 67,
  // A registration certificate could not be fetched from the PCA due to
  // attestation not being available.
  kMetricEnrollmentRegistrationCertificateFetchNotAvailable = 68,
  // Max value for use with enumeration histogram UMA functions.
  kMaxValue = kMetricEnrollmentRegistrationCertificateFetchNotAvailable
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
inline constexpr char kMetricUserPolicyRefresh[] = "Enterprise.PolicyRefresh2";

inline constexpr char kMetricUserPolicyInvalidations[] =
    "Enterprise.PolicyInvalidations";

inline constexpr char kMetricDevicePolicyRefresh[] =
    "Enterprise.DevicePolicyRefresh3";

inline constexpr char kMetricDevicePolicyInvalidations[] =
    "Enterprise.DevicePolicyInvalidations2";

inline constexpr char kMetricDeviceLocalAccountPolicyRefresh[] =
    "Enterprise.DeviceLocalAccountPolicyRefresh3";

inline constexpr char kMetricDeviceLocalAccountPolicyInvalidations[] =
    "Enterprise.DeviceLocalAccountPolicyInvalidations2";

inline constexpr char kMetricCBCMPolicyRefresh[] =
    "Enterprise.CBCMPolicyRefresh";

inline constexpr char kMetricCBCMPolicyInvalidations[] =
    "Enterprise.CBCMPolicyInvalidations";

inline constexpr char kMetricUserRemoteCommandInvalidations[] =
    "Enterprise.UserRemoteCommandInvalidations";
inline constexpr char kMetricDeviceRemoteCommandInvalidations[] =
    "Enterprise.DeviceRemoteCommandInvalidations";
inline constexpr char kMetricCBCMRemoteCommandInvalidations[] =
    "Enterprise.CBCMRemoteCommandInvalidations";

inline constexpr char kMetricUserRemoteCommandReceived[] =
    "Enterprise.UserRemoteCommand.Received";

// Expands to:
// Enterprise.UserRemoteCommand.Executed.CommandEchoTest
// Enterprise.UserRemoteCommand.Executed.DeviceReboot
// Enterprise.UserRemoteCommand.Executed.DeviceScreenshot
// Enterprise.UserRemoteCommand.Executed.DeviceSetVolume
// Enterprise.UserRemoteCommand.Executed.DeviceStartCrdSession
// Enterprise.UserRemoteCommand.Executed.DeviceFetchStatus
// Enterprise.UserRemoteCommand.Executed.UserArcCommand
// Enterprise.UserRemoteCommand.Executed.DeviceWipeUsers
// Enterprise.UserRemoteCommand.Executed.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.UserRemoteCommand.Executed.DeviceRemotePowerwash
// Enterprise.UserRemoteCommand.Executed.DeviceGetAvailableDiagnosticRoutines
// Enterprise.UserRemoteCommand.Executed.DeviceRunDiagnosticRoutine
// Enterprise.UserRemoteCommand.Executed.DeviceGetDiagnosticRoutineUpdate
// Enterprise.UserRemoteCommand.Executed.BrowserClearBrowsingData
// Enterprise.UserRemoteCommand.Executed.DeviceResetEuicc
// Enterprise.UserRemoteCommand.Executed.BrowserRotateAttestationCredential
// Enterprise.UserRemoteCommand.Executed.FetchCrdAvailabilityInfo
// Enterprise.UserRemoteCommand.Executed.FetchSupportPacket
inline constexpr char kMetricUserRemoteCommandExecutedTemplate[] =
    "Enterprise.UserRemoteCommand.Executed.%s";

inline constexpr char kMetricDeviceRemoteCommandReceived[] =
    "Enterprise.DeviceRemoteCommand.Received";

// Expands To:
// Enterprise.DeviceRemoteCommand.Crd.Unknown.UnknownUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.AutoLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.ManuallyLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.AffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.UnaffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.ManagedGuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.GuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.Unknown.NoUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.UnknownUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.AutoLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.ManuallyLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.AffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.UnaffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.ManagedGuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.GuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.NoUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.UnknownUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.AutoLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.ManuallyLaunchedKioskSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.AffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.UnaffiliatedUserSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.ManagedGuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.GuestSession.Result
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.NoUserSession.Result
inline constexpr char kMetricDeviceRemoteCommandCrdResultTemplate[] =
    "Enterprise.DeviceRemoteCommand.Crd.%s.%s.Result";

// ExpandsTo:
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.UnknownUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.AutoLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.ManuallyLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.AffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.UnaffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.ManagedGuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.GuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteAccess.NoUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.UnknownUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.AutoLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.ManuallyLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.AffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.UnaffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.ManagedGuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.GuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.RemoteSupport.NoUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.UnknownUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.AutoLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.ManuallyLaunchedKioskSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.AffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.UnaffiliatedUserSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.ManagedGuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.GuestSession.SessionDuration
// Enterprise.DeviceRemoteCommand.Crd.Unknown.NoUserSession.SessionDuration
inline constexpr char kMetricDeviceRemoteCommandCrdSessionDurationTemplate[] =
    "Enterprise.DeviceRemoteCommand.Crd.%s.%s.SessionDuration";

// Expands to:
// Enterprise.DeviceRemoteCommand.Executed.CommandEchoTest
// Enterprise.DeviceRemoteCommand.Executed.DeviceReboot
// Enterprise.DeviceRemoteCommand.Executed.DeviceScreenshot
// Enterprise.DeviceRemoteCommand.Executed.DeviceSetVolume
// Enterprise.DeviceRemoteCommand.Executed.DeviceStartCrdSession
// Enterprise.DeviceRemoteCommand.Executed.DeviceFetchStatus
// Enterprise.DeviceRemoteCommand.Executed.UserArcCommand
// Enterprise.DeviceRemoteCommand.Executed.DeviceWipeUsers
// Enterprise.DeviceRemoteCommand.Executed.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.DeviceRemoteCommand.Executed.DeviceRemotePowerwash
// Enterprise.DeviceRemoteCommand.Executed.DeviceGetAvailableDiagnosticRoutines
// Enterprise.DeviceRemoteCommand.Executed.DeviceRunDiagnosticRoutine
// Enterprise.DeviceRemoteCommand.Executed.DeviceGetDiagnosticRoutineUpdate
// Enterprise.DeviceRemoteCommand.Executed.BrowserClearBrowsingData
// Enterprise.DeviceRemoteCommand.Executed.DeviceResetEuicc
// Enterprise.DeviceRemoteCommand.Executed.BrowserRotateAttestationCredential
// Enterprise.DeviceRemoteCommand.Executed.FetchCrdAvailabilityInfo
inline constexpr char kMetricDeviceRemoteCommandExecutedTemplate[] =
    "Enterprise.DeviceRemoteCommand.Executed.%s";

inline constexpr char kMetricCBCMRemoteCommandReceived[] =
    "Enterprise.CBCMRemoteCommand.Received";

// Expands to:
// Enterprise.CBCMRemoteCommand.Executed.CommandEchoTest
// Enterprise.CBCMRemoteCommand.Executed.DeviceReboot
// Enterprise.CBCMRemoteCommand.Executed.DeviceScreenshot
// Enterprise.CBCMRemoteCommand.Executed.DeviceSetVolume
// Enterprise.CBCMRemoteCommand.Executed.DeviceStartCrdSession
// Enterprise.CBCMRemoteCommand.Executed.DeviceFetchStatus
// Enterprise.CBCMRemoteCommand.Executed.UserArcCommand
// Enterprise.CBCMRemoteCommand.Executed.DeviceWipeUsers
// Enterprise.CBCMRemoteCommand.Executed.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.CBCMRemoteCommand.Executed.DeviceRemotePowerwash
// Enterprise.CBCMRemoteCommand.Executed.DeviceGetAvailableDiagnosticRoutines
// Enterprise.CBCMRemoteCommand.Executed.DeviceRunDiagnosticRoutine
// Enterprise.CBCMRemoteCommand.Executed.DeviceGetDiagnosticRoutineUpdate
// Enterprise.CBCMRemoteCommand.Executed.BrowserClearBrowsingData
// Enterprise.CBCMRemoteCommand.Executed.DeviceResetEuicc
// Enterprise.CBCMRemoteCommand.Executed.BrowserRotateAttestationCredential
// Enterprise.CBCMRemoteCommand.Executed.FetchCrdAvailabilityInfo
inline constexpr char kMetricCBCMRemoteCommandExecutedTemplate[] =
    "Enterprise.CBCMRemoteCommand.Executed.%s";

// Private set membership UMA histogram names.
inline constexpr char kUMAPsmSuccessTime[] =
    "Enterprise.AutoEnrollmentPrivateSetMembershipSuccessTime";
inline constexpr char kUMAPsmResult[] = "Enterprise.AutoEnrollmentPsmResult";
inline constexpr char kUMAPsmNetworkErrorCode[] =
    "Enterprise.AutoEnrollmentPsmRequestNetworkErrorCode";
inline constexpr char kUMAPsmDmServerRequestStatus[] =
    "Enterprise.AutoEnrollmentPsmDmServerRequestStatus";

// DeviceAutoEnrollmentRequest i.e. hash dance request UMA histogram names.
inline constexpr char kUMAHashDanceSuccessTime[] =
    "Enterprise.AutoEnrollmentHashDanceSuccessTime";
// The following histogram names where added before PSM (private set membership)
// existed. They are only recorded for hash dance.
inline constexpr char kUMAHashDanceProtocolTime[] =
    "Enterprise.AutoEnrollmentProtocolTime";
inline constexpr char kUMAHashDanceBucketDownloadTime[] =
    "Enterprise.AutoEnrollmentBucketDownloadTime";
inline constexpr char kUMAHashDanceRequestStatus[] =
    "Enterprise.AutoEnrollmentRequestStatus";
inline constexpr char kUMAHashDanceNetworkErrorCode[] =
    "Enterprise.AutoEnrollmentRequestNetworkErrorCode";

// The following UMA suffixes are used by Hash dance and PSM protocols.
// Suffix for initial enrollment.
inline constexpr char kUMASuffixInitialEnrollment[] = ".InitialEnrollment";
// Suffix for Forced Re-Enrollment.
inline constexpr char kUMASuffixFRE[] = ".ForcedReenrollment";

// Histograms for the unified state determination.
inline constexpr char kUMAStateDeterminationDeviceIdentifierStatus[] =
    "Enterprise.StateDetermination.DeviceIdentifierStatus";
inline constexpr char kUMAStateDeterminationEnabled[] =
    "Enterprise.StateDetermination.Enabled";
inline constexpr char kUMAStateDeterminationOnFlex[] =
    "Enterprise.StateDetermination.OnFlex";
inline constexpr char kUMAStateDeterminationOwnershipStatus[] =
    "Enterprise.StateDetermination.OwnershipStatus";
inline constexpr char kUMAStateDeterminationPsmReportedAvailableState[] =
    "Enterprise.StateDetermination.PsmReportedAvailableState";
inline constexpr char kUMAStateDeterminationPsmRlweOprfRequestDmStatusCode[] =
    "Enterprise.StateDetermination.PsmRlweOprfRequest.DmStatusCode";
inline constexpr char
    kUMAStateDeterminationPsmRlweOprfRequestNetworkErrorCode[] =
    "Enterprise.StateDetermination.PsmRlweOprfRequest.NetworkErrorCode";
inline constexpr char kUMAStateDeterminationPsmRlweQueryRequestDmStatusCode[] =
    "Enterprise.StateDetermination.PsmRlweQueryRequest.DmStatusCode";
inline constexpr char
    kUMAStateDeterminationPsmRlweQueryRequestNetworkErrorCode[] =
    "Enterprise.StateDetermination.PsmRlweQueryRequest.NetworkErrorCode";
inline constexpr char kUMAStateDeterminationStateKeysRetrievalErrorType[] =
    "Enterprise.StateDetermination.StateKeys.RetrievalErrorType";
inline constexpr char kUMAStateDeterminationStateRequestDmStatusCode[] =
    "Enterprise.StateDetermination.StateRequest.DmStatusCode";
inline constexpr char kUMAStateDeterminationStateRequestNetworkErrorCode[] =
    "Enterprise.StateDetermination.StateRequest.NetworkErrorCode";
inline constexpr char kUMAStateDeterminationStateReturned[] =
    "Enterprise.StateDetermination.StateReturned";
inline constexpr char kUMAStateDeterminationStepDuration[] =
    "Enterprise.StateDetermination.StepDuration";
inline constexpr char kUMAStateDeterminationTotalDurationByState[] =
    "Enterprise.StateDetermination.TotalDurationByState";
inline constexpr char kUMAStateDeterminationTotalDuration[] =
    "Enterprise.StateDetermination.TotalDuration";
inline constexpr char kUMAStateDeterminationStatus[] =
    "Enterprise.StateDetermination.Status";
inline constexpr char kUMAStateDeterminationIsInitialByState[] =
    "Enterprise.StateDetermination.IsInitialByState";

inline constexpr char kUMAPrefixEnrollmentTokenBasedOOBEConfig[] =
    "Enterprise.TokenBasedEnrollmentOobeConfig";

// Suffixes added to kUMAStateDeterminationTotalDurationByState.
inline constexpr char kUMASuffixConnectionError[] = ".ConnectionError";
inline constexpr char kUMASuffixStateKeysRetrievalError[] =
    ".StateKeysRetrievalError";
inline constexpr char kUMASuffixDisabled[] = ".Disabled";
inline constexpr char kUMASuffixEnrollment[] = ".Enrollment";
inline constexpr char kUMASuffixNoEnrollment[] = ".NoEnrollment";
inline constexpr char kUMASuffixServerError[] = ".ServerError";

// Suffixes added to kUMAStateDeterminationStepDuration.
inline constexpr char kUMASuffixOPRFRequest[] = ".OPRFRequest";
inline constexpr char kUMASuffixOwnershipCheck[] = ".OwnershipCheck";
inline constexpr char kUMASuffixQueryRequest[] = ".QueryRequest";
inline constexpr char kUMASuffixStateKeysRetrieval[] = ".StateKeysRetrieval";
inline constexpr char kUMASuffixStateRequest[] = ".StateRequest";

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
