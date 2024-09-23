// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace web_app {

#if !BUILDFLAG(IS_WIN)

// This block defines stub implementations of OS specific methods for
// uninstallation command. Currently, only Windows has its own implementation.
bool ShouldRegisterUninstallationViaOsSettingsWithOs() {
  return false;
}

bool RegisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool UnregisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path) {
  DUMP_WILL_BE_NOTREACHED();
  return true;
}

#endif  // !BUILDFLAG(IS_WIN)

}  // namespace web_app
