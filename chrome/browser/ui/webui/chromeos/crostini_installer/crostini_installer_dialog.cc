// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {
// The dialog content area size. Note that the height is less than the design
// spec to compensate for title bar height.
constexpr int kDialogWidth = 768;
constexpr int kDialogHeight = 608;

GURL GetUrl() {
  return GURL{chrome::kChromeUICrostiniInstallerUrl};
}
}  // namespace

namespace chromeos {

void CrostiniInstallerDialog::Show(Profile* profile,
                                   OnLoadedCallback on_loaded_callback) {
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    return;
  }

  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  // TODO(lxj): Move installer status tracking into the CrostiniInstaller.
  DCHECK(!crostini::CrostiniManager::GetForProfile(profile)
              ->GetCrostiniDialogStatus(crostini::DialogType::INSTALLER));
  crostini::CrostiniManager::GetForProfile(profile)->SetCrostiniDialogStatus(
      crostini::DialogType::INSTALLER, true);

  instance =
      new CrostiniInstallerDialog(profile, std::move(on_loaded_callback));
  instance->ShowSystemDialog();
}

CrostiniInstallerDialog::CrostiniInstallerDialog(
    Profile* profile,
    OnLoadedCallback on_loaded_callback)
    : SystemWebDialogDelegate(GetUrl(), /*title=*/{}),
      profile_(profile),
      on_loaded_callback_(std::move(on_loaded_callback)) {}

CrostiniInstallerDialog::~CrostiniInstallerDialog() {
  crostini::CrostiniManager::GetForProfile(profile_)->SetCrostiniDialogStatus(
      crostini::DialogType::INSTALLER, false);
}

void CrostiniInstallerDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

bool CrostiniInstallerDialog::ShouldShowCloseButton() const {
  return true;
}

bool CrostiniInstallerDialog::ShouldShowDialogTitle() const {
  return true;
}

// TODO(crbug.com/1053376): We should add a browser test for the dialog to check
// that <esc> or X button in overview mode cannot close the dialog immediately
// without the web page noticing it.
bool CrostiniInstallerDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

void CrostiniInstallerDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;

  const ash::ShelfID shelf_id(crostini::kCrostiniInstallerShelfId);
  params->init_properties_container.SetProperty(ash::kShelfIDKey,
                                                shelf_id.Serialize());
}

bool CrostiniInstallerDialog::OnDialogCloseRequested() {
  return installer_ui_ == nullptr || installer_ui_->RequestClosePage();
}

void CrostiniInstallerDialog::OnDialogShown(content::WebUI* webui) {
  installer_ui_ = static_cast<CrostiniInstallerUI*>(webui->GetController());
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void CrostiniInstallerDialog::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  installer_ui_ = nullptr;
  return SystemWebDialogDelegate::OnCloseContents(source, out_close_dialog);
}

void CrostiniInstallerDialog::OnWebContentsFinishedLoad() {
  DCHECK(dialog_window());
  dialog_window()->SetTitle(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_TITLE));
  if (!on_loaded_callback_.is_null()) {
    DCHECK(installer_ui_);
    std::move(on_loaded_callback_).Run(installer_ui_);
  }
}

}  // namespace chromeos
