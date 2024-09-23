// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"

#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace policy::local_user_files {

// Dialog size.
constexpr gfx::Size kDialogSize{448, 360};

// static
bool LocalFilesMigrationDialog::Show(CloudProvider cloud_provider,
                                     base::Time migration_start_time,
                                     base::OnceClosure migration_callback) {
  ash::SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUILocalFilesMigrationURL);
  if (existing_dialog) {
    // TODO(aidazolic): Check params & maybe update title.
    existing_dialog->StackAtTop();
    return false;
  }
  // This pointer is deleted in `SystemWebDialogDelegate::OnDialogClosed`.
  LocalFilesMigrationDialog* dialog = new LocalFilesMigrationDialog(
      cloud_provider, migration_start_time, std::move(migration_callback));
  dialog->ShowSystemDialog();
  return true;
}

// static
LocalFilesMigrationDialog* LocalFilesMigrationDialog::GetDialog() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUILocalFilesMigrationURL);
  return static_cast<LocalFilesMigrationDialog*>(dialog);
}

LocalFilesMigrationDialog::LocalFilesMigrationDialog(
    CloudProvider cloud_provider,
    base::Time migration_start_time,
    base::OnceClosure migration_callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUILocalFilesMigrationURL),
                              /*title=*/std::u16string()),
      cloud_provider_(cloud_provider),
      migration_start_time_(std::move(migration_start_time)),
      migration_callback_(std::move(migration_callback)) {
  set_dialog_size(kDialogSize);

  // This callback runs just before destroying this instance.
  RegisterOnDialogClosedCallback(
      base::BindOnce(&LocalFilesMigrationDialog::ProcessDialogClosing,
                     base::Unretained(this)));
}

LocalFilesMigrationDialog::~LocalFilesMigrationDialog() = default;

gfx::NativeWindow LocalFilesMigrationDialog::GetDialogWindowForTesting() const {
  CHECK_IS_TEST();
  return dialog_window();
}

void LocalFilesMigrationDialog::OnDialogShown(content::WebUI* webui) {
  CHECK(migration_callback_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  static_cast<LocalFilesMigrationUI*>(webui->GetController())
      ->SetInitialDialogInfo(cloud_provider_, migration_start_time_);
}

bool LocalFilesMigrationDialog::ShouldShowCloseButton() const {
  return false;
}

ui::mojom::ModalType LocalFilesMigrationDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kWindow;
}

void LocalFilesMigrationDialog::ProcessDialogClosing(
    const std::string& ret_value) {
  // If closed because user clicked on "Upload now", start the migration.
  if (ret_value == kStartMigration) {
    if (!migration_callback_) {
      LOG(ERROR) << "Upload now clicked, but migration callback is empty!";
      return;
    }
    std::move(migration_callback_).Run();
  }
}

}  // namespace policy::local_user_files
