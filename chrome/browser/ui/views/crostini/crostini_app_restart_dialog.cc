// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_restart_dialog.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace crostini {

namespace {

gfx::NativeWindow GetNativeWindowFromDisplayId(int64_t display_id) {
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display;
  screen->GetDisplayWithDisplayId(display_id, &display);
  return screen->GetWindowAtScreenPoint(display.bounds().origin());
}

std::unique_ptr<views::View> MakeCrostiniAppRestartView() {
  auto view = std::make_unique<views::View>();

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_APP_RESTART_BODY);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  view->AddChildView(message_label);

  return view;
}

std::unique_ptr<views::DialogDelegate> MakeCrostiniAppRestartDelegate(
    std::unique_ptr<views::View> contents) {
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->set_internal_name("CrostiniAppRestart");
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  delegate->SetContentsView(std::move(contents));
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  delegate->SetOwnedByWidget(true);
  delegate->SetShowCloseButton(false);
  delegate->set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  return delegate;
}

void ShowInternal(gfx::NativeWindow context) {
  auto contents = MakeCrostiniAppRestartView();
  auto delegate = MakeCrostiniAppRestartDelegate(std::move(contents));
  views::DialogDelegate::CreateDialogWidget(std::move(delegate), context,
                                            nullptr)
      ->Show();
}

}  // namespace

void ShowAppRestartDialog(int64_t display_id) {
  ShowInternal(GetNativeWindowFromDisplayId(display_id));
}

void ShowAppRestartDialogForTesting(gfx::NativeWindow context) {
  ShowInternal(context);
}

}  // namespace crostini
