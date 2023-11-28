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
constexpr gfx::Size kDialogSize{768, 608};

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
    : SystemWebDialogDelegate(url, /*title=*/{}) {
  set_dialog_size(kDialogSize);
  set_show_close_button(true);
  set_close_dialog_on_escape(false);
  set_show_dialog_title(false);
}

GuestOSInstallerDialog::~GuestOSInstallerDialog() = default;

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
