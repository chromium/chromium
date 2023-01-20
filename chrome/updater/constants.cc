// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/constants.h"

#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

// App ids.
const char kUpdaterAppId[] = "{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}";
const char kQualificationAppId[] = "{6f0f9a34-a0ab-4a75-a0eb-6eab78d0dc4b}";

const char kNullVersion[] = "0.0.0.0";

#if BUILDFLAG(IS_WIN)
const char kExecutableName[] = "updater.exe";
#else
const char kExecutableName[] = "updater";
#endif

// Command line arguments.
// If a command line switch is marked as `backward-compatibility`, it
// means the switch name cannot be changed, and the parser must be able to
// handle command line in the DOS style '/<switch> <optional_value>'. This is to
// make sure the new updater understands the hand-off requests from the legacy
// updaters.
const char kServerSwitch[] = "server";
const char kWindowsServiceSwitch[] = "windows-service";
const char kComServiceSwitch[] = "com-service";
const char kCrashMeSwitch[] = "crash-me";
const char kCrashHandlerSwitch[] = "crash-handler";
const char kUpdateSwitch[] = "update";
const char kInstallSwitch[] = "install";
const char kRuntimeSwitch[] = "runtime";
const char kUninstallSwitch[] = "uninstall";
const char kUninstallSelfSwitch[] = "uninstall-self";
const char kUninstallIfUnusedSwitch[] = "uninstall-if-unused";
const char kSystemSwitch[] = "system";
const char kTestSwitch[] = "test";
const char kInitDoneNotifierSwitch[] = "init-done-notifier";
const char kNoRateLimitSwitch[] = "no-rate-limit";
const char kEnableLoggingSwitch[] = "enable-logging";
const char kLoggingModuleSwitch[] = "vmodule";
const char kLoggingModuleSwitchValue[] =
#if BUILDFLAG(IS_WIN)
    "*/components/winhttp/*=2,"
#endif
    "*/components/update_client/*=2,*/chrome/updater/*=2";
const char kAppIdSwitch[] = "app-id";
const char kAppVersionSwitch[] = "app-version";
const char kWakeSwitch[] = "wake";
const char kWakeAllSwitch[] = "wake-all";
const char kTagSwitch[] = "tag";
const char kInstallerDataSwitch[] = "installerdata";

const char kServerServiceSwitch[] = "service";

const char kServerUpdateServiceInternalSwitchValue[] = "update-internal";
const char kServerUpdateServiceSwitchValue[] = "update";

// Recovery command line arguments.
const char kRecoverSwitch[] = "recover";
const char kBrowserVersionSwitch[] = "browser-version";
const char kSessionIdSwitch[] = "sessionid";  // backward-compatibility.
const char kAppGuidSwitch[] = "appguid";

const char kHealthCheckSwitch[] = "healthcheck";

const char kEnterpriseSwitch[] = "enterprise";  // backward-compatibility.
const char kSilentSwitch[] = "silent";          // backward-compatibility.
const char kHandoffSwitch[] = "handoff";        // backward-compatibility.
const char kOfflineDirSwitch[] = "offlinedir";  // backward-compatibility.
const char kAppArgsSwitch[] = "appargs";        // backward-compatibility.

const char kCmdLineExpectElevated[] = "expect-elevated";

const char kCmdLineExpectDeElevated[] = "expect-de-elevated";

const char kCmdLinePrefersUser[] = "prefers-user";

// Path names.
const char kAppsDir[] = "apps";
const char kUninstallScript[] = "uninstall.cmd";

// Developer override key names.
const char kDevOverrideKeyUrl[] = "url";
const char kDevOverrideKeyUseCUP[] = "use_cup";
const char kDevOverrideKeyInitialDelay[] = "initial_delay";
const char kDevOverrideKeyServerKeepAliveSeconds[] = "server_keep_alive";
const char kDevOverrideKeyCrxVerifierFormat[] = "crx_verifier_format";
const char kDevOverrideKeyGroupPolicies[] = "group_policies";
const char kDevOverrideKeyOverinstallTimeout[] = "overinstall_timeout";

// Developer override file name, relative to app data directory.
const char kDevOverrideFileName[] = "overrides.json";

// Policy Management constants.
const char kProxyModeDirect[] = "direct";
const char kProxyModeAutoDetect[] = "auto_detect";
const char kProxyModePacScript[] = "pac_script";
const char kProxyModeFixedServers[] = "fixed_servers";
const char kProxyModeSystem[] = "system";

// Specifies that urls that can be cached by proxies are preferred.
const char kDownloadPreferenceCacheable[] = "cacheable";

const char kUTF8BOM[] = "\xEF\xBB\xBF";

const char kSourceGroupPolicyManager[] = "GroupPolicy";
const char kSourceDMPolicyManager[] = "DeviceManagement";
const char kSourceManagedPreferencePolicyManager[] = "ManagedPreference";
const char kSourceDefaultValuesPolicyManager[] = "default";
const char kSourceDictValuesPolicyManager[] = "DictValuePolicy";

#if BUILDFLAG(IS_MAC)
// The user defaults suite name.
const char kUserDefaultsSuiteName[] = MAC_BUNDLE_IDENTIFIER_STRING ".defaults";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace updater
