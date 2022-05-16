// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppRegistrar;

// Returns the app id for the given System App |type|.
absl::optional<AppId> GetAppIdForSystemApp(
    const WebAppRegistrar& registrar,
    const ash::SystemAppDelegateMap& delegates,
    SystemAppType type);

// Returns the System App Type for the given |app_id|.
absl::optional<SystemAppType> GetSystemAppTypeForAppId(
    const WebAppRegistrar& registrar,
    const ash::SystemAppDelegateMap& delegates,
    const AppId& app_id);

// Returns whether |app_id| points to an installed System App.
bool IsSystemWebApp(const WebAppRegistrar& registrar,
                    const ash::SystemAppDelegateMap& delegates,
                    const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_
