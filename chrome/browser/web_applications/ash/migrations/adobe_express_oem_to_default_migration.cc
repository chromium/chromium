// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/ash/migrations/adobe_express_oem_to_default_migration.h"

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app::migrations {

void MigrateAdobeExpressFromOemInstallToDefault(WebAppSyncBridge* sync_bridge) {
  if (!sync_bridge->registrar().IsInstalled(kAdobeExpressAppId)) {
    return;
  }

  if (!sync_bridge->registrar()
           .GetAppById(kAdobeExpressAppId)
           ->GetSources()
           .Has(WebAppManagement::Type::kOem)) {
    return;
  }

  web_app::ScopedRegistryUpdate update = sync_bridge->BeginUpdate();
  web_app::WebApp* app = update->UpdateApp(kAdobeExpressAppId);
  CHECK(app);

  app->AddSource(WebAppManagement::Type::kApsDefault);
  app->RemoveSource(WebAppManagement::Type::kOem);
}

}  // namespace web_app::migrations
