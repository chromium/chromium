// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/views/borealis/borealis_disallowed_dialog.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"

namespace {
// The dialog content area size. Note that the height is less than the design
// spec to compensate for title bar height.
constexpr int kDialogWidth = 805;
constexpr int kDialogHeight = 520;

GURL GetUrl() {
  return GURL{chrome::kChromeUIBorealisInstallerUrl};
}

}  // namespace

namespace ash {

void BorealisInstallerDialog::Show(Profile* profile,
                                   OnLoadedCallback on_loaded_callback) {
  borealis::BorealisServiceFactory::GetForProfile(profile)
      ->Features()
      .IsAllowed(base::BindOnce(
          &BorealisInstallerDialog::ShowBorealisInstallerDialogIfAllowed,
          profile, std::move(on_loaded_callback)));
}

void BorealisInstallerDialog::ShowBorealisInstallerDialogIfAllowed(
    Profile* profile,
    OnLoadedCallback on_loaded_callback,
    borealis::BorealisFeatures::AllowStatus status) {
  if (status != borealis::BorealisFeatures::AllowStatus::kAllowed) {
    views::borealis::ShowInstallerDisallowedDialog(status);
    return;
  }
  auto* instance = ash::SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  instance =
      new ash::BorealisInstallerDialog(profile, std::move(on_loaded_callback));
  instance->ShowSystemDialog();
}

BorealisInstallerDialog::BorealisInstallerDialog(
    Profile* profile,
    OnLoadedCallback on_loaded_callback)
    : SystemWebDialogDelegate(GetUrl(), /*title=*/{}),
      profile_(profile),
      on_loaded_callback_(std::move(on_loaded_callback)) {}

BorealisInstallerDialog::~BorealisInstallerDialog() {}

void BorealisInstallerDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

bool BorealisInstallerDialog::ShouldShowCloseButton() const {
  return true;
}

bool BorealisInstallerDialog::ShouldShowDialogTitle() const {
  return true;
}

bool BorealisInstallerDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

void BorealisInstallerDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;

  params->init_properties_container.SetProperty(
      kShelfIDKey, ash::ShelfID(borealis::kInstallerAppId).Serialize());
  params->init_properties_container.SetProperty<int>(kShelfItemTypeKey,
                                                     TYPE_DIALOG);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  params->init_properties_container.SetProperty(
      aura::client::kAppIconKey,
      rb.GetImageNamed(IDR_LOGO_BOREALIS_STEAM_192).AsImageSkia());
}

bool BorealisInstallerDialog::OnDialogCloseRequested() {
  return !installer_ui_ || installer_ui_->RequestClosePage();
}

void BorealisInstallerDialog::OnDialogShown(content::WebUI* webui) {
  installer_ui_ =
      static_cast<BorealisInstallerUI*>(webui->GetController())->GetWeakPtr();
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void BorealisInstallerDialog::OnWebContentsFinishedLoad() {
  DCHECK(dialog_window());
  dialog_window()->SetTitle(
      l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_APP_NAME));
  if (!on_loaded_callback_.is_null()) {
    DCHECK(installer_ui_);
    std::move(on_loaded_callback_).Run(installer_ui_);
  }
}

}  // namespace ash
