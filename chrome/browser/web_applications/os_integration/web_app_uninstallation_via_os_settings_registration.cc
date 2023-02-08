// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"

#include "base/check.h"
#include "build/build_config.h"

namespace web_app {

#if !BUILDFLAG(IS_WIN)

// This block defines stub implementations of OS specific methods for
// uninstallation command. Currently, only Windows has their own implementation.
bool ShouldRegisterUninstallationViaOsSettingsWithOs() {
  return false;
}

void RegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                               const std::string& app_name,
                                               Profile* profile) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());
  // Stub function for OS's which don't register uninstallation command with the
  // OS.
}

void UnegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                                Profile* profile) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());
  // Stub function for OS's which don't register uninstallation command with the
  // OS.
}

#endif  // !BUILDFLAG(IS_WIN)

}  // namespace web_app
