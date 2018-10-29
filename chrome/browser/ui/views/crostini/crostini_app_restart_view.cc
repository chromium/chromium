// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_restart_view.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

gfx::NativeWindow GetNativeWindowFromDisplayId(int64_t display_id) {
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display;
  screen->GetDisplayWithDisplayId(display_id, &display);
  return screen->GetWindowAtScreenPoint(display.bounds().origin());
}

}  // namespace

// static
void CrostiniAppRestartView::Show(const ash::ShelfID& id, int64_t display_id) {
  CrostiniAppRestartView* view = new CrostiniAppRestartView(id, display_id);
  if (features::IsUsingWindowService()) {
    // TODO(mash): Simplify specifying |display_id| via CreateDialogWidget, etc.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params =
        views::DialogDelegate::GetDialogWidgetInitParams(view, nullptr, nullptr,
                                                         gfx::Rect());
    params.mus_properties[ws::mojom::WindowManager::kDisplayId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(display_id);
    widget->Init(params);
  } else {
    // TODO(timzheng): Remove this after single process mash is enabled.
    views::DialogDelegate::CreateDialogWidget(
        view, GetNativeWindowFromDisplayId(display_id), nullptr);
  }
  view->GetWidget()->Show();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_APP_RESTART);
}

int CrostiniAppRestartView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
}

base::string16 CrostiniAppRestartView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_CROSTINI_APP_RESTART_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_CROSTINI_NOT_NOW_BUTTON);
}

bool CrostiniAppRestartView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniAppRestartView::Accept() {
  ChromeLauncherController::instance()
      ->crostini_app_window_shelf_controller()
      ->Restart(id_, display_id_);
  return true;  // Should close the dialog
}

gfx::Size CrostiniAppRestartView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniAppRestartView::CrostiniAppRestartView(const ash::ShelfID& id,
                                               int64_t display_id)
    : id_(id), display_id_(display_id) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  const base::string16 device_type = ui::GetChromeOSDeviceName();
  const base::string16 message =
      l10n_util::GetStringFUTF16(IDS_CROSTINI_APP_RESTART_BODY, device_type);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);
}

ui::ModalType CrostiniAppRestartView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}
