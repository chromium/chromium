// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"

#include <string>

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "url/gurl.h"

namespace ash::extended_updates {

ExtendedUpdatesDialog::~ExtendedUpdatesDialog() = default;

void ExtendedUpdatesDialog::Show() {
  ExtendedUpdatesDialog* dialog = ExtendedUpdatesDialog::Get();
  if (dialog) {
    dialog->Focus();
    return;
  }
  dialog = new ExtendedUpdatesDialog();
  dialog->ShowSystemDialog();
}

ExtendedUpdatesDialog* ExtendedUpdatesDialog::Get() {
  return static_cast<ExtendedUpdatesDialog*>(
      SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUIExtendedUpdatesDialogURL));
}

ExtendedUpdatesDialog::ExtendedUpdatesDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIExtendedUpdatesDialogURL),
                              std::u16string()) {}

}  // namespace ash::extended_updates
