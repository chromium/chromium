// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shimless_rma_dialog/shimless_rma_dialog.h"

#include <string>

#include "ash/webui/shimless_rma/url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

// static
void ShimlessRmaDialog::ShowDialog() {
  ShimlessRmaDialog* dialog = new ShimlessRmaDialog();
  dialog->ShowSystemDialog(nullptr);
}

ShimlessRmaDialog::ShimlessRmaDialog()
    : SystemWebDialogDelegate(GURL(kChromeUIShimlessRMAUrl),
                              /*title=*/std::u16string()) {
  // MODAL_TYPE_SYSTEM renders over OOBE/login screens, but does not support
  // ui::mojom::WindowShowState::kFullscreen correctly.
  // This dialog uses DisplayObserver::OnDisplayMetricsChanged to update the
  // window size as screen size changes.
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
  set_can_minimize(false);
}

ShimlessRmaDialog::~ShimlessRmaDialog() = default;

std::string ShimlessRmaDialog::Id() {
  return id_;
}

void ShimlessRmaDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  // This overrides the class name so TAST tests can find the root node.
  params->name = "ShimlessRmaDialogView";
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
  params->visible_on_all_workspaces = true;
  params->corner_radius = 0;
  params->show_state = ui::mojom::WindowShowState::kFullscreen;
  params->remove_standard_frame = true;
  params->opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
}

void ShimlessRmaDialog::GetDialogSize(gfx::Size* size) const {
  *size = display::Screen::GetScreen()->GetPrimaryDisplay().size();
}

bool ShimlessRmaDialog::ShouldShowCloseButton() const {
  return false;
}

bool ShimlessRmaDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

bool ShimlessRmaDialog::CanMaximizeDialog() const {
  return false;
}

void ShimlessRmaDialog::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  dialog_window()->SetBounds(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
}

}  // namespace ash
