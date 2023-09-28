// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/publisher_helper.h"

#include "base/feature_list.h"

#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

webapps::WebappUninstallSource ConvertUninstallSourceToWebAppUninstallSource(
    apps::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::UninstallSource::kAppList:
      return webapps::WebappUninstallSource::kAppList;
    case apps::UninstallSource::kAppManagement:
      return webapps::WebappUninstallSource::kAppManagement;
    case apps::UninstallSource::kShelf:
      return webapps::WebappUninstallSource::kShelf;
    case apps::UninstallSource::kMigration:
      return webapps::WebappUninstallSource::kMigration;
    case apps::UninstallSource::kUnknown:
      return webapps::WebappUninstallSource::kUnknown;
  }
}

bool IsAppServiceShortcut(const webapps::AppId& web_app_id,
                          const WebAppProvider& provider) {
// TODO(crbug.com/1412708): Support Lacros.
// Currently, we will only treat shortcuts differently in ash. Lacros support
// will be added later, and we will send the flag value over to Lacros to
// determine whether we publish shortcut differently, so that there will be only
// one flag controlling the shortcut feature. On non-ChromeOS platforms,
// shortcuts will still be published as web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) {
    return provider.registrar_unsafe().IsInstalled(web_app_id) &&
           provider.registrar_unsafe().IsShortcutApp(web_app_id);
  }
#endif
  return false;
}

}  // namespace web_app
