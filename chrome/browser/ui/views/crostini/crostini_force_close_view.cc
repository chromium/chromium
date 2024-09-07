// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_force_close_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
    gfx::NativeWindow context,
    gfx::NativeView parent,
    base::OnceClosure force_close_callback) {
  views::Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new CrostiniForceCloseView(base::UTF8ToUTF16(app_name),
                                 std::move(force_close_callback)),
      context, parent);
  dialog_widget->Show();
  return dialog_widget;
}

CrostiniForceCloseView::CrostiniForceCloseView(
    const std::u16string& app_name,
    base::OnceClosure force_close_callback) {
  SetShowCloseButton(false);
  SetTitle(
      app_name.empty()
          ? l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_TITLE_UNKNOWN)
          : l10n_util::GetStringFUTF16(IDS_CROSTINI_FORCE_CLOSE_TITLE_KNOWN,
                                       app_name));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_ACCEPT_BUTTON));
  SetAcceptCallback(std::move(force_close_callback));

  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  views::Label* message_label = new views::Label(
      app_name.empty()
          ? l10n_util::GetStringUTF16(IDS_CROSTINI_FORCE_CLOSE_BODY_UNKNOWN)
          : l10n_util::GetStringFUTF16(IDS_CROSTINI_FORCE_CLOSE_BODY_KNOWN,
                                       app_name));
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);

  set_close_on_deactivate(true);
}

CrostiniForceCloseView::~CrostiniForceCloseView() = default;

BEGIN_METADATA(CrostiniForceCloseView)
END_METADATA
