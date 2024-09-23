// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

namespace policy::local_user_files {

// The action signaling the user clicked on "Upload now" and migration should
// start.
inline constexpr char kStartMigration[] = "start-migration";

using StartMigrationCallback = base::OnceClosure;

// Declares the WebUI-based dialog for local files migration.
class LocalFilesMigrationDialog : public ash::SystemWebDialogDelegate {
 public:
  // Shows the Local Files Migration dialog.
  //
  // If a dialog is already open, brings it to the front and returns false.
  // Otherwise, shows the dialog and returns true.
  static bool Show(CloudProvider cloud_provider,
                   base::Time migration_start_time,
                   StartMigrationCallback migration_callback);

  // Returns a pointer to the instance of LocalFilesMigrationDialog, if it
  // exists.
  static LocalFilesMigrationDialog* GetDialog();

  LocalFilesMigrationDialog(const LocalFilesMigrationDialog&) = delete;
  LocalFilesMigrationDialog& operator=(const LocalFilesMigrationDialog&) =
      delete;

  // Returns the native window. Should only be used in tests.
  gfx::NativeWindow GetDialogWindowForTesting() const;

  // ash::SystemWebDialogDelegate implementation:
  void OnDialogShown(content::WebUI* webui) override;

 private:
  LocalFilesMigrationDialog(CloudProvider cloud_provider,
                            base::Time migration_start_time,
                            StartMigrationCallback migration_callback);
  ~LocalFilesMigrationDialog() override;

  // ash::SystemWebDialogDelegate implementation:
  bool ShouldShowCloseButton() const override;
  ui::mojom::ModalType GetDialogModalType() const override;

  // Called when the dialog is closed. If `ret-value` is set to kStartMigration,
  // the user clicked "Upload now" and uploads should start.
  void ProcessDialogClosing(const std::string& ret_value);

  // Cloud provider to which files are uploaded.
  CloudProvider cloud_provider_;

  // The time at which migration automatically starts.
  base::Time migration_start_time_;

  // Called if the user starts migration immediately.
  StartMigrationCallback migration_callback_;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_DIALOG_H_
