// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "url/gurl.h"

namespace policy::local_user_files {

void LocalFilesMigrationDialog::Show(base::TimeDelta migration_delay,
                                     base::OnceClosure migration_callback) {
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUILocalFilesMigrationURL))) {
    return;
  }

  LocalFilesMigrationDialog* dialog = new LocalFilesMigrationDialog();
  dialog->ShowSystemDialog();
}

LocalFilesMigrationDialog::LocalFilesMigrationDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUILocalFilesMigrationURL),
                              /*title=*/std::u16string()) {
  // TODO(b/342340599): Set appropriate height when the text is finalized.
  set_dialog_size(gfx::Size(SystemWebDialogDelegate::kDialogWidth,
                            SystemWebDialogDelegate::kDialogHeight));
}

LocalFilesMigrationDialog::~LocalFilesMigrationDialog() = default;

bool LocalFilesMigrationDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace policy::local_user_files
