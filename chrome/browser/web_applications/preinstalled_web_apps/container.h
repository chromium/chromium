// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_CONTAINER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_CONTAINER_H_

#include <optional>

#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

namespace web_app {

// Returns the config for preinstalling the container app.
ExternalInstallOptions GetConfigForContainer(
    const std::optional<DeviceInfo>& device_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_CONTAINER_H_
