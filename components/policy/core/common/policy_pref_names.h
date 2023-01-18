// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_

#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace policy_prefs {

#if BUILDFLAG(IS_WIN)
POLICY_EXPORT extern const char kAzureActiveDirectoryManagement[];
POLICY_EXPORT extern const char kEnterpriseMDMManagementWindows[];
#endif
POLICY_EXPORT extern const char kCloudManagementEnrollmentMandatory[];
POLICY_EXPORT extern const char kDlpClipboardCheckSizeLimit[];
POLICY_EXPORT extern const char kDlpReportingEnabled[];
POLICY_EXPORT extern const char kDlpRulesList[];
#if BUILDFLAG(IS_MAC)
POLICY_EXPORT extern const char kEnterpriseMDMManagementMac[];
POLICY_EXPORT extern const char kScreenTimeEnabled[];
#endif
POLICY_EXPORT extern const char kLastPolicyStatisticsUpdate[];
POLICY_EXPORT extern const char kNativeWindowOcclusionEnabled[];
POLICY_EXPORT extern const char kSafeSitesFilterBehavior[];
POLICY_EXPORT extern const char kSystemFeaturesDisableList[];
POLICY_EXPORT extern const char kSystemFeaturesDisableMode[];
POLICY_EXPORT extern const char kUrlBlocklist[];
POLICY_EXPORT extern const char kUrlAllowlist[];
POLICY_EXPORT extern const char kUserPolicyRefreshRate[];
POLICY_EXPORT extern const char kIntensiveWakeUpThrottlingEnabled[];
POLICY_EXPORT extern const char kUserAgentClientHintsGREASEUpdateEnabled[];
POLICY_EXPORT extern const char kUnthrottledNestedTimeoutEnabled[];
#if BUILDFLAG(IS_ANDROID)
POLICY_EXPORT extern const char kBackForwardCacheEnabled[];
#endif  // BUILDFLAG(IS_ANDROID)
POLICY_EXPORT extern const char kIsolatedAppsDeveloperModeAllowed[];
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
POLICY_EXPORT extern const char kLastPolicyCheckTime[];
#endif
#if BUILDFLAG(IS_IOS)
POLICY_EXPORT extern const char kUserPolicyNotificationWasShown[];
#endif
POLICY_EXPORT extern const char kEventPathEnabled[];
POLICY_EXPORT extern const char kOffsetParentNewSpecBehaviorEnabled[];
POLICY_EXPORT extern const char kSendMouseEventsDisabledFormControlsEnabled[];
POLICY_EXPORT extern const char kUseMojoVideoDecoderForPepperAllowed[];
POLICY_EXPORT extern const char kPPAPISharedImagesSwapChainAllowed[];
POLICY_EXPORT extern const char kForceEnablePepperVideoDecoderDevAPI[];

}  // namespace policy_prefs
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
