// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/constants.h"

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
const char kSystemSwitch[] = "system";
const char kTestSwitch[] = "test";
const char kInitDoneNotifierSwitch[] = "init-done-notifier";
const char kNoRateLimitSwitch[] = "no-rate-limit";
const char kEnableLoggingSwitch[] = "enable-logging";
const char kLoggingModuleSwitch[] = "vmodule";
const char kAppIdSwitch[] = "app-id";
const char kAppVersionSwitch[] = "app-version";
const char kWakeSwitch[] = "wake";

const char kServerServiceSwitch[] = "service";
const char kServerControlServiceSwitchValue[] = "control";
const char kServerUpdateServiceSwitchValue[] = "update";

#if defined(OS_WIN)
const char kInstallFromOutDir[] = "install-from-out-dir";
#endif  // OS_WIN

// TODO(crbug/1108975): brand the URLs below.
const char kUpdaterJSONDefaultUrl[] =
    "https://update.googleapis.com/service/update2/json";
const char kCrashUploadURL[] = "https://clients2.google.com/cr/report";
const char kCrashStagingUploadURL[] =
    "https://clients2.google.com/cr/staging_report";
const char kDeviceManagementServerURL[] =
    "https://m.google.com/devicemanagement/data/api";

// Path names.
const char kAppsDir[] = "apps";
const char kUninstallScript[] = "uninstall.cmd";

// Developer override key names.
const char kDevOverrideKeyUrl[] = "url";
const char kDevOverrideKeyUseCUP[] = "use_cup";

// Policy Management constants.
const char kProxyModeDirect[] = "direct";
const char kProxyModeAutoDetect[] = "auto_detect";
const char kProxyModePacScript[] = "pac_script";
const char kProxyModeFixedServers[] = "fixed_servers";
const char kProxyModeSystem[] = "system";

// Specifies that urls that can be cached by proxies are preferred.
const char kDownloadPreferenceCacheable[] = "cacheable";

}  // namespace updater
