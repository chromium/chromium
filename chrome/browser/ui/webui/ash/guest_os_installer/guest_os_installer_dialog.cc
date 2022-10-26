// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/guest_os_installer/guest_os_installer_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace {
// The dialog content area size. Note that the height is less than the design
// spec to compensate for title bar height.
constexpr int kDialogWidth = 768;
constexpr int kDialogHeight = 608;

}  // namespace

namespace ash {

void GuestOSInstallerDialog::Show(const GURL& page_url) {
  auto* instance = SystemWebDialogDelegate::FindInstance(page_url.spec());
  if (instance) {
    instance->Focus();
    return;
  }

  instance = new GuestOSInstallerDialog(page_url);
  instance->ShowSystemDialog();
}

GuestOSInstallerDialog::GuestOSInstallerDialog(const GURL& url)
    : SystemWebDialogDelegate(url, /*title=*/{}) {}

GuestOSInstallerDialog::~GuestOSInstallerDialog() = default;

void GuestOSInstallerDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

std::u16string GuestOSInstallerDialog::GetDialogTitle() const {
  // Set GuestOS specific title here
  return {};
}

bool GuestOSInstallerDialog::ShouldShowCloseButton() const {
  return true;
}

bool GuestOSInstallerDialog::ShouldShowDialogTitle() const {
  return false;
}

bool GuestOSInstallerDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

void GuestOSInstallerDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;

  const ash::ShelfID shelf_id(Id());
  params->init_properties_container.SetProperty(ash::kShelfIDKey,
                                                shelf_id.Serialize());
  params->init_properties_container.SetProperty<int>(ash::kShelfItemTypeKey,
                                                     ash::TYPE_DIALOG);

  gfx::ImageSkia image;
  // Load GuestOS shelf icons here
  params->init_properties_container.SetProperty(aura::client::kAppIconKey,
                                                image);
}

}  // namespace ash
