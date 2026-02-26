// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RESOLVE_WEB_APP_PENDING_MIGRATION_INFO_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RESOLVE_WEB_APP_PENDING_MIGRATION_INFO_COMMAND_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"

namespace web_app {

// This command finds all apps that have a validated migration 'source' (e.g.,
// they are the target of the migration), and ensures that all of those 'source'
// apps have the target app as a pending migration.
//
// Due to this operation being about relationships between apps, this acquires
// an 'AllAppsLock' to make sure it has exclusive access to all apps.
//
// Note: If a source app has a pending migration, but the target app is not
// installed (or somehow not found in the registry), this command will clear
// that pending migration from the source app. This ensures that pending
// migrations are always consistent with the currently installed apps.
class ResolveWebAppPendingMigrationInfoCommand
    : public WebAppCommand<AllAppsLock> {
 public:
  explicit ResolveWebAppPendingMigrationInfoCommand(base::OnceClosure callback);
  ~ResolveWebAppPendingMigrationInfoCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  std::unique_ptr<AllAppsLock> lock_;
  base::WeakPtrFactory<ResolveWebAppPendingMigrationInfoCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RESOLVE_WEB_APP_PENDING_MIGRATION_INFO_COMMAND_H_
