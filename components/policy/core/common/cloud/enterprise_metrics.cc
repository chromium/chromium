// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace policy {

const char kMetricUserPolicyRefresh[] = "Enterprise.PolicyRefresh2";
const char kMetricUserPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.PolicyRefresh2";

const char kMetricUserPolicyInvalidations[] = "Enterprise.PolicyInvalidations";
const char kMetricUserPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidations";

const char kMetricDevicePolicyRefresh[] = "Enterprise.DevicePolicyRefresh3";
const char kMetricDevicePolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyRefresh3";

const char kMetricDevicePolicyInvalidations[] =
    "Enterprise.DevicePolicyInvalidations2";
const char kMetricDevicePolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyInvalidations2";

const char kMetricDeviceLocalAccountPolicyRefresh[] =
    "Enterprise.DeviceLocalAccountPolicyRefresh3";
const char kMetricDeviceLocalAccountPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.DeviceLocalAccountPolicyRefresh3";

const char kMetricDeviceLocalAccountPolicyInvalidations[] =
    "Enterprise.DeviceLocalAccountPolicyInvalidations2";
const char kMetricDeviceLocalAccountPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.DeviceLocalAccountPolicyInvalidations2";

const char kMetricCBCMPolicyRefresh[] = "Enterprise.CBCMPolicyRefresh";
const char kMetricCBCMPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.CBCMPolicyRefresh";

const char kMetricCBCMPolicyInvalidations[] =
    "Enterprise.CBCMPolicyInvalidations";
const char kMetricCBCMPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.CBCMPolicyInvalidations";

const char kMetricPolicyInvalidationRegistration[] =
    "Enterprise.PolicyInvalidationsRegistrationResult";
const char kMetricPolicyInvalidationRegistrationFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidationsRegistrationResult";

const char kMetricUserRemoteCommandInvalidations[] =
    "Enterprise.UserRemoteCommandInvalidations";
const char kMetricDeviceRemoteCommandInvalidations[] =
    "Enterprise.DeviceRemoteCommandInvalidations";
const char kMetricCBCMRemoteCommandInvalidations[] =
    "Enterprise.CBCMRemoteCommandInvalidations";

const char kMetricRemoteCommandInvalidationsRegistrationResult[] =
    "Enterprise.RemoteCommandInvalidationsRegistrationResult";

const char kMetricUserRemoteCommandReceived[] =
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
const char kMetricUserRemoteCommandExecutedTemplate[] =
    "Enterprise.UserRemoteCommand.Executed.%s";

const char kMetricDeviceRemoteCommandReceived[] =
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
const char kMetricDeviceRemoteCommandCrdResultTemplate[] =
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
const char kMetricDeviceRemoteCommandCrdSessionDurationTemplate[] =
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
const char kMetricDeviceRemoteCommandExecutedTemplate[] =
    "Enterprise.DeviceRemoteCommand.Executed.%s";

const char kMetricCBCMRemoteCommandReceived[] =
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
const char kMetricCBCMRemoteCommandExecutedTemplate[] =
    "Enterprise.CBCMRemoteCommand.Executed.%s";

const char kUMAPsmSuccessTime[] =
    "Enterprise.AutoEnrollmentPrivateSetMembershipSuccessTime";
const char kUMAPsmResult[] = "Enterprise.AutoEnrollmentPsmResult";
const char kUMAPsmNetworkErrorCode[] =
    "Enterprise.AutoEnrollmentPsmRequestNetworkErrorCode";
const char kUMAPsmDmServerRequestStatus[] =
    "Enterprise.AutoEnrollmentPsmDmServerRequestStatus";

const char kUMAHashDanceSuccessTime[] =
    "Enterprise.AutoEnrollmentHashDanceSuccessTime";
const char kUMAHashDanceProtocolTime[] =
    "Enterprise.AutoEnrollmentProtocolTime";
const char kUMAHashDanceBucketDownloadTime[] =
    "Enterprise.AutoEnrollmentBucketDownloadTime";
const char kUMAHashDanceRequestStatus[] =
    "Enterprise.AutoEnrollmentRequestStatus";
const char kUMAHashDanceNetworkErrorCode[] =
    "Enterprise.AutoEnrollmentRequestNetworkErrorCode";

const char kUMASuffixInitialEnrollment[] = ".InitialEnrollment";
const char kUMASuffixFRE[] = ".ForcedReenrollment";

}  // namespace policy
