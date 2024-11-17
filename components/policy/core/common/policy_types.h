// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_

namespace policy {

// The scope of a policy flags whether it is meant to be applied to the current
// user or to the machine.  Note that this property pertains to the source of
// the policy and has no direct correspondence to the distinction between User
// Policy and Device Policy.
enum PolicyScope {
  // USER policies apply to sessions of the current user.
  POLICY_SCOPE_USER,

  // MACHINE policies apply to any users of the current machine.
  POLICY_SCOPE_MACHINE,

  // Number of scope types. Has to be last element.
  POLICY_SCOPE_MAX = POLICY_SCOPE_MACHINE
};

// The level of a policy determines its enforceability and whether users can
// override it or not. The values are listed in increasing order of priority.
enum PolicyLevel {
  // RECOMMENDED policies can be overridden by users. They are meant as a
  // default value configured by admins, that users can customize.
  POLICY_LEVEL_RECOMMENDED,

  // MANDATORY policies must be enforced and users can't circumvent them.
  POLICY_LEVEL_MANDATORY,

  // Number of level types. Has to be equal to last element.
  POLICY_LEVEL_MAX = POLICY_LEVEL_MANDATORY
};

// The source of a policy indicates where its value is originating from. The
// sources are ordered by priority (with weakest policy first).
enum PolicySource {
  // The policy was set because we are running in an enterprise environment.
  POLICY_SOURCE_ENTERPRISE_DEFAULT,

  // The policy was set by command line flag for testing purpose.
  POLICY_SOURCE_COMMAND_LINE,

  // The policy was set by a cloud source.
  POLICY_SOURCE_CLOUD,

  // The policy was set by an Active Directory source.
  POLICY_SOURCE_ACTIVE_DIRECTORY,

  // Deprecated. Not removed to avoid disturbing the reporting enum mapping.
  // Any non-platform policy was overridden because we are running in a
  // public session or kiosk mode.
  POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE_DEPRECATED,

  // The policy was set by a platform source.
  POLICY_SOURCE_PLATFORM,

  // Deprecated. Not removed to avoid disturbing the reporting enum mapping.
  // The policy was set by a cloud source that has higher priroity.
  POLICY_SOURCE_PRIORITY_CLOUD_DEPRECATED,

  // The policy coming from multiple sources and its value has been merged.
  POLICY_SOURCE_MERGED,

  // The policy was set by Cloud in Ash and piped to Lacros.
  POLICY_SOURCE_CLOUD_FROM_ASH,

  // The policy was set by the DeviceRestrictedManagedGuestSessionEnabled
  // policy. This source should be kept as highest priority source.
  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,

  // Number of source types. Has to be the last element.
  POLICY_SOURCE_COUNT
};

// The priorities are set in order, with lowest priority first. A similar enum
// doesn't exist for Chrome OS since there is a 1:1 mapping from source to
// priority, so source is used directly in the priority comparison.
enum PolicyPriorityBrowser {
  // The policy was set through remapping or debugging.
  POLICY_PRIORITY_BROWSER_ENTERPRISE_DEFAULT,

  // The policy was set by command line flag for testing purposes.
  POLICY_PRIORITY_BROWSER_COMMAND_LINE,

  // The policy was set by a cloud source at the user level.
  POLICY_PRIORITY_BROWSER_CLOUD_USER,

  // The policy was set by a platform source at the user level.
  POLICY_PRIORITY_BROWSER_PLATFORM_USER,

  // The policy was set by a cloud source at the machine level.
  POLICY_PRIORITY_BROWSER_CLOUD_MACHINE,

  // The policy was set by a cloud source at the user level. Its priority is
  // raised above that of cloud machine policies.
  POLICY_PRIORITY_BROWSER_CLOUD_USER_RAISED,

  // The policy was set by a platform source at the machine level.
  POLICY_PRIORITY_BROWSER_PLATFORM_MACHINE,

  // The policy was set by a platform source at the machine level. Its priority
  // is raised above that of platform machine policies.
  POLICY_PRIORITY_BROWSER_CLOUD_MACHINE_RAISED,

  // The policy was set by a cloud source at the user level. Its priority is
  // raised above that of platform and cloud machine policies.
  POLICY_PRIORITY_BROWSER_CLOUD_USER_DOUBLE_RAISED,

  // The policy coming from multiple sources and its value has been merged.
  POLICY_PRIORITY_BROWSER_MERGED,
};

// The `PolicyFetchReason` enum is used to tag requests to DMServer. This allows
// for more targeted monitoring and alerting.
enum class PolicyFetchReason {
  kUnspecified,
  // Policy fetched at browser start, e.g. device policies at Ash start.
  kBrowserStart,
  // Policy fetches triggered by the Chrome Remote Desktop policy watcher.
  kCrdHostPolicyWatcher,
  // Required policy fetch during device enrollment.
  kDeviceEnrollment,
  // Policy fetch triggered by incoming FCM invalidation.
  kInvalidation,
  // ChromeOS policy fetch request from Lacros to Ash.
  kLacros,
  // CloudPolicyClient registration changed (e.g. during startup).
  kRegistrationChanged,
  // Various retry reasons based on DMServer status, see
  // `CloudPolicyRefreshScheduler`.
  kRetryAfterStatusServiceActivationPending,
  kRetryAfterStatusServicePolicyNotFound,
  kRetryAfterStatusServiceTooManyRequests,
  kRetryAfterStatusRequestFailed,
  kRetryAfterStatusTemporaryUnavailable,
  kRetryAfterStatusCannotSignRequest,
  kRetryAfterStatusRequestInvalid,
  kRetryAfterStatusHttpStatusError,
  kRetryAfterStatusResponseDecodingError,
  kRetryAfterStatusServiceManagementNotSupported,
  kRetryAfterStatusRequestTooLarge,
  // Scheduled policy refresh (e.g. once per day).
  kScheduled,
  // Policy fetch triggered by sign-in.
  kSignin,
  // A placeholder reason used for tests (should not occur in production).
  kTest,
  // Policy fetched as requested by the user, e.g. through chrome://policy.
  kUserRequest,
  // Policy fetch as previous request failed.
  kRetry,
  // Schema Update
  kSchemaUpdated,
  // Disconnect from cloud management
  kDisconnect,
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_
