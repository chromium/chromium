// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"

namespace web_app {

const char kUrlKey[] = "url";
const char kDefaultLaunchContainerKey[] = "default_launch_container";
const char kDefaultLaunchContainerWindowValue[] = "window";
const char kDefaultLaunchContainerTabValue[] = "tab";
const char kCreateDesktopShortcutKey[] = "create_desktop_shortcut";
const char kFallbackAppNameKey[] = "fallback_app_name";
const char kCustomNameKey[] = "custom_name";
const char kCustomIconKey[] = "custom_icon";
const char kCustomIconURLKey[] = "url";
const char kCustomIconHashKey[] = "hash";
const char kInstallAsShortcut[] = "install_as_shortcut";
const char kUninstallAndReplaceKey[] = "uninstall_and_replace";

const char kWildcard[] = "*";

const char kManifestId[] = "manifest_id";
const char kRunOnOsLogin[] = "run_on_os_login";
const char kAllowed[] = "allowed";
const char kBlocked[] = "blocked";
const char kRunWindowed[] = "run_windowed";
const char kPreventClose[] = "prevent_close_after_run_on_os_login";
const char kForceUnregisterOsIntegration[] = "force_unregister_os_integration";

}  // namespace web_app
