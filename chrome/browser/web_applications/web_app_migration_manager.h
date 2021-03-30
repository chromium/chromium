// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace syncer {
class ModelError;
class MetadataBatch;
}  // namespace syncer

namespace extensions {
class BookmarkAppRegistrar;
class BookmarkAppRegistryController;
class BookmarkAppIconManager;
class BookmarkAppFileHandlerManager;
}  // namespace extensions

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebAppDatabase;
class WebAppIconManager;

// Migrates all bookmark apps to new web apps registry.
class WebAppMigrationManager {
 public:
  WebAppMigrationManager(Profile* profile,
                         AbstractWebAppDatabaseFactory* database_factory,
                         WebAppIconManager* web_app_icon_manager,
                         OsIntegrationManager* os_integration_manager);
  WebAppMigrationManager(const WebAppMigrationManager&) = delete;
  WebAppMigrationManager& operator=(const WebAppMigrationManager&) = delete;
  ~WebAppMigrationManager();

  using MigrationCompletedCallback = base::OnceCallback<void(bool success)>;
  void StartDatabaseMigration(
      MigrationCompletedCallback migration_completed_callback);

 private:
  void OnWebAppDatabaseOpened(
      Registry web_app_registry,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnBookmarkAppRegistryReady(Registry web_app_registry);

  // Migrates next bookmark app in |bookmark_app_ids_| queue or starts
  // the registry migration if the queue is empty.
  void MigrateNextBookmarkAppIcons();
  void OnBookmarkAppIconsRead(const AppId& app_id, IconBitmaps icon_bitmaps);
  void OnWebAppIconsWritten(const AppId& app_id, bool success);
  void OnBookmarkAppShortcutsMenuIconsRead(
      const AppId& app_id,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);
  void OnWebAppShortcutsMenuIconsWritten(bool success);

  void MigrateBookmarkAppInstallSource(const AppId& app_id, WebApp* web_app);
  bool CanMigrateBookmarkApp(const AppId& app_id) const;
  std::unique_ptr<WebApp> MigrateBookmarkApp(const AppId& app_id);
  void MigrateBookmarkAppsRegistry();

  void OnWebAppRegistryWritten(bool success);

  void ReportDatabaseError(const syncer::ModelError& error);

  // We don't want to destruct database_ object immediately in callbacks from
  // WebAppDatabase. This would be a violation of the caller/callee contract.
  // We should use PostTask instead.
  void ScheduleDestructDatabaseAndCallCallback(bool success);
  void DestructDatabaseAndCallCallback(bool success);

  std::unique_ptr<extensions::BookmarkAppRegistrar> bookmark_app_registrar_;
  std::unique_ptr<extensions::BookmarkAppRegistryController>
      bookmark_app_registry_controller_;
  std::unique_ptr<extensions::BookmarkAppIconManager>
      bookmark_app_icon_manager_;
  std::unique_ptr<extensions::BookmarkAppFileHandlerManager>
      bookmark_app_file_handler_manager_;

  AbstractWebAppDatabaseFactory* const database_factory_;
  WebAppIconManager* const web_app_icon_manager_;
  std::unique_ptr<WebAppDatabase> database_;

  // A queue of bookmark app ids to be migrated.
  std::vector<AppId> bookmark_app_ids_;
  // Current bookmark app id which icons are being migrated.
  std::vector<AppId>::const_iterator next_app_id_iterator_;

  MigrationCompletedCallback migration_completed_callback_;

  base::WeakPtrFactory<WebAppMigrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_MANAGER_H_
