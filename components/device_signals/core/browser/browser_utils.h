// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BROWSER_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BROWSER_UTILS_H_

#include <optional>

#include "build/build_config.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

class PolicyBlocklistService;
class PrefService;

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace device_signals {

bool GetChromeRemoteDesktopAppBlocked(PolicyBlocklistService* service);

std::optional<safe_browsing::PasswordProtectionTrigger>
GetPasswordProtectionWarningTrigger(PrefService* profile_prefs);

safe_browsing::SafeBrowsingState GetSafeBrowsingProtectionLevel(
    PrefService* profile_prefs);

std::optional<std::string> TryGetEnrollmentDomain(
    policy::CloudPolicyManager* manager);

bool GetSiteIsolationEnabled();

#if !BUILDFLAG(IS_ANDROID)
// Returns the hostname of the current machine.
std::string GetHostName();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Returns the hostname of the current machine.
std::vector<std::string> GetSystemDnsServers();

// Returns the current state of the OS firewall.
SettingValue GetOSFirewall();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
// Returns the path to the ufw configuration file.
const char** GetUfwConfigPath();
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
// Returns the domain of the current Windows user.
std::optional<std::string> GetWindowsUserDomain();

// Returns the machine GUID of the current Windows machine.
std::optional<std::string> GetMachineGuid();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Get the last date a security patch is applied on the device, in the format of
// milliseconds since epoch.
std::optional<int64_t> GetSecurityPatchLevelEpoch();
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BROWSER_UTILS_H_
