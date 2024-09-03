// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace policy::local_user_files {

// Actions a user can take in the migration dialog.
enum class UserAction {
  kDismiss,
  kUploadNow,
};

// Callback invoked when the user interacts with the dialog.
using UserActionCallback = base::OnceCallback<void(UserAction)>;

// Handles communication and logic for the Local Files Migration WebUI page.
class LocalFilesMigrationPageHandler : public mojom::PageHandler {
 public:
  LocalFilesMigrationPageHandler(
      content::WebUI* web_ui,
      Profile* profile,
      CloudProvider cloud_provider,
      base::Time migration_start_time,
      UserActionCallback callback,
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver);

  LocalFilesMigrationPageHandler(const LocalFilesMigrationPageHandler&) =
      delete;
  LocalFilesMigrationPageHandler& operator=(
      const LocalFilesMigrationPageHandler&) = delete;

  ~LocalFilesMigrationPageHandler() override;

  // mojom::PageHandler implementation:
  // Fetches initial information to display in the dialog.
  void GetInitialDialogInfo(GetInitialDialogInfoCallback callback) override;
  // Closes the dialog and initiates the file upload immediately.
  void UploadNow() override;
  // Closes the dialog without initiating the upload.
  void Close() override;

 private:
  // Periodically updates the remaining time displayed in the UI.
  void UpdateRemainingTime();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUI> web_ui_;
  CloudProvider cloud_provider_;
  base::Time migration_start_time_;
  base::WallClockTimer ui_update_timer_;
  UserActionCallback callback_;

  // Mojo communication
  mojo::Receiver<PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;

  base::WeakPtrFactory<LocalFilesMigrationPageHandler> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_PAGE_HANDLER_H_
