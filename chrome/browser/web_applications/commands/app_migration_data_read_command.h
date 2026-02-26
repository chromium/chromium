// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_MIGRATION_DATA_READ_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_MIGRATION_DATA_READ_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

using UpdateMetadata = std::optional<WebAppIdentityUpdate>;

// Parse a web app's pending migration metadata and icons stored on the disk to
// construct a `WebAppIdentityUpdate` instance that can be use to show the app
// identity update dialog.
class AppMigrationDataReadCommand
    : public WebAppCommand<AppLock, UpdateMetadata> {
 public:
  AppMigrationDataReadCommand(
      const webapps::AppId& old_app_id,
      const webapps::AppId& new_app_id,
      bool is_forced_migration_on_startup,
      base::OnceCallback<void(UpdateMetadata)> completed_callback);
  ~AppMigrationDataReadCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void SetOldIconForIdentityUpdate(SkBitmap old_icon);
  void SetNewIconForIdentityUpdate(SkBitmap new_icon);
  void ReadSingleIcon(const webapps::AppId& app_id,
                      base::OnceCallback<void(SkBitmap)> callback);
  void OnIconsProcessedCreateIdentity();

  std::unique_ptr<AppLock> lock_;
  const webapps::AppId old_app_id_;
  const webapps::AppId new_app_id_;
  WebAppIdentityUpdate update_;

  base::WeakPtrFactory<AppMigrationDataReadCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_MIGRATION_DATA_READ_COMMAND_H_
