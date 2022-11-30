// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"

class Profile;

namespace web_app {

// True if uninstallation via os settings are managed externally by the
// operating system. Windows is the only Os that support this feature now.
bool ShouldRegisterUninstallationViaOsSettingsWithOs();

// Do OS-specific registration for the web app. The registration writes
// an entry to the global uninstall location in the Windows registry.
// Once an entry exists in the given Windows registry, it will be
// displayed in the Windows OS settings so that user can uninstall from
// there like any other native apps.
void RegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                               const std::string& app_name,
                                               Profile* profile);

// Remove an entry from the Windows uninstall registry.
void UnegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                                Profile* profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_UNINSTALLATION_VIA_OS_SETTINGS_REGISTRATION_H_
