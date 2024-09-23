// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/publisher_helper.h"

#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

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
  return false;
}

}  // namespace web_app
