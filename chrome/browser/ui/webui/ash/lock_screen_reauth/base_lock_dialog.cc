// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/base_lock_dialog.h"

#include "chrome/common/webui_url_constants.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

gfx::Size FitSizeToDisplay(const gfx::Size& desired) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();
  display_size.SetToMin(desired);
  return display_size;
}

}  // namespace

constexpr gfx::Size BaseLockDialog::kBaseLockDialogSize;

BaseLockDialog::BaseLockDialog(const GURL& url, const gfx::Size& desired_size)
    : SystemWebDialogDelegate(url, /*title=*/std::u16string()),
      desired_size_(desired_size) {}

BaseLockDialog::~BaseLockDialog() = default;

void BaseLockDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(desired_size_);
}

void BaseLockDialog::AdjustWidgetInitParams(views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::mojom::ModalType BaseLockDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kSystem;
}

}  // namespace ash
