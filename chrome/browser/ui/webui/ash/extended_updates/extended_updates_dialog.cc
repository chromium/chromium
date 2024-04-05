// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"

#include <string>

#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "url/gurl.h"

namespace {
GURL GetUrl() {
  return GURL(chrome::kChromeUIExtendedUpdatesDialogURL);
}
}  // namespace

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
      SystemWebDialogDelegate::FindInstance(GetUrl().spec()));
}

void ExtendedUpdatesDialog::GetDialogSize(gfx::Size* size) const {
  *size = CalculateOobeDialogSizeForPrimaryDisplay();
}

bool ExtendedUpdatesDialog::ShouldShowCloseButton() const {
  // Closing the dialog is done via the web ui.
  return false;
}

ExtendedUpdatesDialog::ExtendedUpdatesDialog()
    : SystemWebDialogDelegate(GetUrl(), std::u16string()) {}

}  // namespace ash::extended_updates
