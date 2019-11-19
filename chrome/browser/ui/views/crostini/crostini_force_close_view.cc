// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_force_close_view.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace crostini {

// Due to chrome's include rules, we implement this function here.
views::Widget* ShowCrostiniForceCloseDialog(
    const std::string& app_name,
    views::Widget* closable_widget,
    base::OnceClosure force_close_callback) {
  return CrostiniForceCloseView::Show(app_name, closable_widget,
                                      std::move(force_close_callback));
}

}  // namespace crostini

views::Widget* CrostiniForceCloseView::Show(
    const std::string& app_name,
    views::Widget* closable_widget,
    base::OnceClosure force_close_callback) {
  return Show(app_name, closable_widget->GetNativeWindow(),
              closable_widget->GetNativeView(),
              std::move(force_close_callback));
}

views::Widget* CrostiniForceCloseView::Show(
    const std::string& app_name,
    gfx::NativeWindow closable_window,
    gfx::NativeView closable_view,
    base::OnceClosure force_close_callback) {
  views::Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new CrostiniForceCloseView(app_name, std::move(force_close_callback)),
      closable_window, closable_view);
  dialog_widget->Show();
  return dialog_widget;
}

bool CrostiniForceCloseView::Accept() {
  std::move(force_close_callback_).Run();
  return true;
}

ui::ModalType CrostiniForceCloseView::GetModalType() const {
  return ui::ModalType::MODAL_TYPE_WINDOW;
}

int CrostiniForceCloseView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool CrostiniForceCloseView::ShouldShowCloseButton() const {
  return false;
}

base::string16 CrostiniForceCloseView::GetWindowTitle() const {
  return app_name_.empty()
             ? l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_TITLE_UNKNOWN)
             : l10n_util::GetStringFUTF16(IDS_CROSTINI_FORCE_CLOSE_TITLE_KNOWN,
                                          app_name_);
}

gfx::Size CrostiniForceCloseView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniForceCloseView::CrostiniForceCloseView(
    const std::string& app_name,
    base::OnceClosure force_close_callback)
    : app_name_(base::UTF8ToUTF16(app_name)),
      force_close_callback_(std::move(force_close_callback)) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_ACCEPT_BUTTON));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  views::Label* message_label = new views::Label(
      app_name_.empty()
          ? l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_BODY_UNKNOWN)
          : l10n_util::GetStringFUTF16(IDS_CROSTINI_FORCE_CLOSE_BODY_KNOWN,
                                       app_name_));
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);

  set_close_on_deactivate(true);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_FORCE_CLOSE);
}

CrostiniForceCloseView::~CrostiniForceCloseView() = default;
