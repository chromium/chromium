// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_

#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class FilePath;
}

namespace web_app {

#if BUILDFLAG(IS_WIN)
std::wstring GetUninstallStringKeyForTesting(const base::FilePath& profile_path,
                                             const webapps::AppId& app_id);
#endif

// True if uninstallation via os settings are managed externally by the
// operating system. Windows is the only Os that support this feature now.
bool ShouldRegisterUninstallationViaOsSettingsWithOs();

// Do OS-specific registration for the web app. The registration writes
// an entry to the global uninstall location in the Windows registry.
// Once an entry exists in the given Windows registry, it will be
// displayed in the Windows OS settings so that user can uninstall from
// there like any other native apps.
// Returns if the operation was successful.
bool RegisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path);

// Remove an entry from the Windows uninstall registry.
// Returns true if the operation had no errors. The registry key not existing is
// not considered an error, and return true.
bool UnregisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_
