// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/constants.h"

#include "chrome/updater/updater_branding.h"

namespace updater {

// App ids.
const char kUpdaterAppId[] = "{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}";

const char kNullVersion[] = "0.0.0.0";

// Command line arguments.
const char kServerSwitch[] = "server";
const char kComServiceSwitch[] = "com-service";
const char kCrashMeSwitch[] = "crash-me";
const char kCrashHandlerSwitch[] = "crash-handler";
const char kUpdateSwitch[] = "update";
const char kInstallSwitch[] = "install";
const char kUninstallSwitch[] = "uninstall";
const char kUninstallSelfSwitch[] = "uninstall-self";
const char kUninstallIfUnusedSwitch[] = "uninstall-if-unused";
const char kSystemSwitch[] = "system";
const char kTestSwitch[] = "test";
const char kInitDoneNotifierSwitch[] = "init-done-notifier";
const char kNoRateLimitSwitch[] = "no-rate-limit";
const char kEnableLoggingSwitch[] = "enable-logging";
const char kLoggingModuleSwitch[] = "vmodule";
const char kAppIdSwitch[] = "app-id";
const char kAppVersionSwitch[] = "app-version";
const char kWakeSwitch[] = "wake";
const char kTagSwitch[] = "tag";

const char kServerServiceSwitch[] = "service";

const char kServerUpdateServiceInternalSwitchValue[] = "update-internal";
const char kServerUpdateServiceSwitchValue[] = "update";

#if defined(OS_WIN)
const char kInstallFromOutDir[] = "install-from-out-dir";
#endif  // OS_WIN

// Path names.
const char kAppsDir[] = "apps";
const char kUninstallScript[] = "uninstall.cmd";

// Developer override key names.
const char kDevOverrideKeyUrl[] = "url";
const char kDevOverrideKeyUseCUP[] = "use_cup";
const char kDevOverrideKeyInitialDelay[] = "initial_delay";
const char kDevOverrideKeyServerKeepAliveSeconds[] = "server_keep_alive";

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

#if defined(OS_MAC)
// The user defaults suite name.
const char kUserDefaultsSuiteName[] = MAC_BUNDLE_IDENTIFIER_STRING ".defaults";
#endif  // defined(OS_MAC)

}  // namespace updater
