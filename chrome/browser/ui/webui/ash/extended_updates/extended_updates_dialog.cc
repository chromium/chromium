// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"

#include <string>

#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "base/logging.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
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

  if (!ExtendedUpdatesController::Get()->IsOptInEligible(
          ProfileManager::GetActiveUserProfile())) {
    if (dialog) {
      LOG(ERROR)
          << "Trying to reshow Extended Updates dialog while not eligible.";
      dialog->Close();
    } else {
      LOG(ERROR)
          << "Trying to show Extended Updates dialog while not eligible.";
    }
    return;
  }

  if (dialog) {
    dialog->dialog_window()->Focus();
    return;
  }
  dialog = new ExtendedUpdatesDialog();
  dialog->ShowSystemDialog();
  RecordExtendedUpdatesDialogEvent(ExtendedUpdatesDialogEvent::kDialogShown);
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
    : SystemWebDialogDelegate(GetUrl(), std::u16string()) {
  set_dialog_modal_type(ui::mojom::ModalType::kWindow);
}

}  // namespace ash::extended_updates
