// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace policy::local_user_files {

// Declares the WebUI-based dialog for local files migration.
class LocalFilesMigrationDialog : public ash::SystemWebDialogDelegate {
 public:
  // Shows the dialog with the given `migration_delay`.
  // `migration_callback` is called if the user chooses to start the migration
  // immediately.
  static void Show(base::TimeDelta migration_delay,
                   base::OnceClosure migration_callback);

  LocalFilesMigrationDialog();
  ~LocalFilesMigrationDialog() override;
  LocalFilesMigrationDialog(const LocalFilesMigrationDialog&) = delete;
  LocalFilesMigrationDialog& operator=(const LocalFilesMigrationDialog&) =
      delete;

 private:
  bool ShouldShowCloseButton() const override;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_
