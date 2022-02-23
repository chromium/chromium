// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_

#include "chrome/browser/web_applications/web_app_id.h"

struct WebAppInstallInfo;

namespace web_app {

struct InstallOsHooksOptions;
class WebAppRegistrar;
class WebApp;

// Updates |web_app| using |web_app_info|
void SetWebAppManifestFields(const WebAppInstallInfo& web_app_info,
                             WebApp& web_app);

// Possibly updates |options| to disable OS-integrations based on the
// configuration of the given app.
void MaybeDisableOsIntegration(const WebAppRegistrar* app_registrar,
                               const AppId& app_id,
                               InstallOsHooksOptions* options);

// Returns true if web app is allowed to update its identity (name and/or icon).
bool CanWebAppUpdateIdentity(const WebApp* web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_
