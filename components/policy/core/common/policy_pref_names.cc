// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_pref_names.h"

#include "build/build_config.h"
#include "policy_pref_names.h"

namespace policy {
namespace policy_prefs {

#if BUILDFLAG(IS_WIN)
// Integer pref that stores Azure Active Directory management authority.
const char kAzureActiveDirectoryManagement[] =
    "management.platform.azure_active_directory";

// Integer pref that stores the Windows enterprise MDM management authority.
const char kEnterpriseMDMManagementWindows[] =
    "management.platform.enterprise_mdm_win";
#elif BUILDFLAG(IS_MAC)
// Integer pref that stores the Mac enterprise MDM management authority.
const char kEnterpriseMDMManagementMac[] =
    "management.platform.enterprise_mdm_mac";
// Boolean pref that indicates whether integration with macOS Screen Time should
// be enabled.
const char kScreenTimeEnabled[] = "policy.screen_time";
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

// Boolean policy preference for force enabling or disabling the
// MaxUnthrottledTimeoutNestingLevel web feature.
const char kUnthrottledNestedTimeoutEnabled[] =
    "policy.unthrottled_nested_timeout";

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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Last time that a check for cloud policy management was done. This time is
// recorded on Android and iOS so that retries aren't attempted on every
// startup. Instead the cloud policy registration is retried at least 1 or 3
// days later.
const char kLastPolicyCheckTime[] = "policy.last_policy_check_time";
#endif

#if BUILDFLAG(IS_IOS)
const char kUserPolicyNotificationWasShown[] =
    "policy.user_policy_notification_was_shown";
#endif

// A boolean indicating whether the deprecated API Event.path is enabled. It
// should eventually be disabled and removed.
// https://chromestatus.com/feature/5726124632965120
const char kEventPathEnabled[] = "policy.event_path_enabled";

// A boolean indicating whether the newly specified behavior for
// Element.offsetParent is in effect.
const char kOffsetParentNewSpecBehaviorEnabled[] =
    "policy.offset_parent_new_spec_behavior_enabled";

// A boolean indicating whether the new behavior for event dispatching on
// disabled form controls is in effect.
const char kSendMouseEventsDisabledFormControlsEnabled[] =
    "policy.send_mouse_events_disabled_form_controls_enabled";

// If true the feature UseMojoVideoDecoderForPepper will be allowed, otherwise
// feature will be forced off.
const char kUseMojoVideoDecoderForPepperAllowed[] =
    "policy.use_mojo_video_decoder_for_pepper_allowed";

// If true the feature PPAPISharedImagesSwapChain will be allowed, otherwise
// feature will be forced off.
const char kPPAPISharedImagesSwapChainAllowed[] =
    "policy.ppapi_shared_images_swap_chain_allowed";

// If true then support for the PPB_VideoDecoder(Dev) API will be enabled;
// otherwise the browser will decide whether the API is supported.
const char kForceEnablePepperVideoDecoderDevAPI[] =
    "policy.force_enable_pepper_video_decoder_dev_api";

}  // namespace policy_prefs
}  // namespace policy
