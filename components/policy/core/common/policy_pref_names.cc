// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_pref_names.h"

#include "build/build_config.h"

namespace policy {
namespace policy_prefs {

#if BUILDFLAG(IS_WIN)
// Boolean pref that stores if the OS is actively managed by
// Azure Active Directory. This will be used to cache the management status so
// that it is loaded faster at sartup.
const char kAzureActiveDirectoryManagement[] =
    "management.platform.azure_active_directory";
#elif BUILDFLAG(IS_MAC)
// Integer pref that stores the Mac enterprise MDM management authority.
const char kEnterpriseMDMManagementMac[] =
    "management.platform.enterprise_mdm_mac";
#endif

// 64-bit serialization of the time last policy usage statistics were collected
// by UMA_HISTOGRAM_ENUMERATION.
const char kLastPolicyStatisticsUpdate[] = "policy.last_statistics_update";

// Enum specifying if/how the SafeSites content filter should be applied.
// See the SafeSitesFilterBehavior policy for details.
const char kSafeSitesFilterBehavior[] = "policy.safe_sites_filter_behavior";

// A list of system features to be disabled (see policy
// "SystemFeaturesDisableList").
const char kSystemFeaturesDisableList[] = "policy.system_features_disable_list";

// Enum specifying the user experience of disabled features.
// See the SystemFeaturesDisableMode policy for details.
const char kSystemFeaturesDisableMode[] = "policy.system_features_disable_mode";

// Blocks access to the listed host patterns.
const char kUrlBlocklist[] = "policy.url_blocklist";

// Allows access to the listed host patterns, as exceptions to the blacklist.
const char kUrlAllowlist[] = "policy.url_allowlist";

// Integer that specifies the policy refresh rate for user-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
const char kUserPolicyRefreshRate[] = "policy.user_refresh_rate";

// Boolean indicates whether the cloud management enrollment is mandatory or
// not.
const char kCloudManagementEnrollmentMandatory[] =
    "policy.cloud_management_enrollment_mandatory";

// Integer that sets the minimal limit on the data size in the clipboard to be
// checked against Data Leak Prevention rules.
const char kDlpClipboardCheckSizeLimit[] =
    "policy.dlp_clipboard_check_size_limit";

// Boolean policy preference to enable reporting of data leak prevention events.
const char kDlpReportingEnabled[] = "policy.dlp_reporting_enabled";

// A list of Data leak prevention rules.
const char kDlpRulesList[] = "policy.dlp_rules_list";

// A boolean value that can be used to disable native window occlusion
// calculation, even if the Finch feature is enabled.
const char kNativeWindowOcclusionEnabled[] =
    "policy.native_window_occlusion_enabled";

// Boolean policy preference for force enabling or disabling the
// IntensiveWakeUpThrottling web feature. Only applied if the policy is managed.
const char kIntensiveWakeUpThrottlingEnabled[] =
    "policy.intensive_wake_up_throttling_enabled";

// Boolean that controls whether a window spawned from an anchor targeting
// _blank receives an opener. TODO(crbug.com/898942): Remove this in Chrome 95.
const char kTargetBlankImpliesNoOpener[] =
    "policy.target_blank_implies_noopener";

#if BUILDFLAG(IS_ANDROID)
// Boolean policy preference to disable the BackForwardCache feature.
const char kBackForwardCacheEnabled[] = "policy.back_forward_cache_enabled";
#endif  // BUILDFLAG(IS_ANDROID)

// Boolean policy preference to disable the User-Agent Client Hints
// updated GREASE algorithm feature.
const char kUserAgentClientHintsGREASEUpdateEnabled[] =
    "policy.user_agent_client_hints_grease_update_enabled";

// Boolean policy to allow isolated apps developer mode.
const char kIsolatedAppsDeveloperModeAllowed[] =
    "policy.isolated_apps_developer_mode_allowed";

// Boolean policy to force WebSQL to be enabled.
const char kWebSQLAccess[] = "policy.web_sql_access";

}  // namespace policy_prefs
}  // namespace policy
