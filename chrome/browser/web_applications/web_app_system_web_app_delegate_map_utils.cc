// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"

#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "url/gurl.h"

namespace web_app {

std::optional<webapps::AppId> GetAppIdForSystemApp(
    const WebAppRegistrar& registrar,
    const ash::SystemWebAppDelegateMap& delegates,
    ash::SystemWebAppType type) {
  const ash::SystemWebAppDelegate* delegate =
      ash::GetSystemWebApp(delegates, type);
  if (!delegate)
    return std::nullopt;

  std::optional<GURL> app_install_url = delegate->GetInstallUrl();
  if (!app_install_url.has_value())
    return std::nullopt;

  return registrar.LookupExternalAppId(app_install_url.value());
}

std::optional<ash::SystemWebAppType> GetSystemAppTypeForAppId(
    const WebAppRegistrar& registrar,
    const ash::SystemWebAppDelegateMap& delegates,
    const webapps::AppId& app_id) {
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app || !web_app->client_data().system_web_app_data.has_value()) {
    return std::nullopt;
  }

  // The registered system apps can change from previous runs (e.g. flipping a
  // SWA flag). The registry isn't up-to-date until SWA finishes installing, so
  // we could have a invalid type (for current session) during SWA install.
  //
  // This check ensures we return a type that is safe for other methods (avoids
  // crashing when looking up that type).
  ash::SystemWebAppType proto_type =
      web_app->client_data().system_web_app_data->system_app_type;
  if (delegates.contains(proto_type)) {
    return proto_type;
  }

  return std::nullopt;
}

bool IsSystemWebApp(const WebAppRegistrar& registrar,
                    const ash::SystemWebAppDelegateMap& delegates,
                    const webapps::AppId& app_id) {
  return GetSystemAppTypeForAppId(registrar, delegates, app_id).has_value();
}

}  // namespace web_app
