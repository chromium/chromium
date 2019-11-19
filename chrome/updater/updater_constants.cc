// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_constants.h"

namespace updater {

// App ids.
const char kUpdaterAppId[] = "{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}";
const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// Command line arguments.
const char kCrashMeSwitch[] = "crash-me";
const char kCrashHandlerSwitch[] = "crash-handler";
const char kInstallSwitch[] = "install";
const char kUninstallSwitch[] = "uninstall";
const char kUpdateAppsSwitch[] = "ua";
const char kTestSwitch[] = "test";
const char kInitDoneNotifierSwitch[] = "init-done-notifier";
const char kNoRateLimitSwitch[] = "no-rate-limit";
const char kEnableLoggingSwitch[] = "enable-logging";
const char kLoggingModuleSwitch[] = "vmodule";

// URLs.
const char kUpdaterJSONDefaultUrl[] =
    "https://update.googleapis.com/service/update2/json";
const char kCrashUploadURL[] = "https://clients2.google.com/cr/report";
const char kCrashStagingUploadURL[] =
    "https://clients2.google.com/cr/staging_report";

// Path names.
extern const char kAppsDir[] = "apps";
extern const char kUninstallScript[] = "uninstall.cmd";

}  // namespace updater
