// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

SettingsOverriddenDialogView::SettingsOverriddenDialogView(
    std::unique_ptr<SettingsOverriddenDialogController> controller)
    : controller_(std::move(controller)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_CHANGE_IT_BACK));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_KEEP_IT));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  using DialogResult = SettingsOverriddenDialogController::DialogResult;
  auto make_result_callback = [this](DialogResult result) {
    // NOTE: The following Bind's are safe because the callback is
    // owned by this object (indirectly, as a DialogDelegate).
    return base::BindOnce(
        &SettingsOverriddenDialogView::NotifyControllerOfResult,
        base::Unretained(this), result);
  };
  SetAcceptCallback(make_result_callback(DialogResult::kChangeSettingsBack));
  SetCancelCallback(make_result_callback(DialogResult::kKeepNewSettings));
  SetCloseCallback(make_result_callback(DialogResult::kDialogDismissed));

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SettingsOverriddenDialogController::ShowParams show_params =
      controller_->GetShowParams();
  SetTitle(show_params.dialog_title);
  if (show_params.icon)
    SetShowIcon(true);

  auto message_label = std::make_unique<views::Label>(
      show_params.message, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(message_label));
}

SettingsOverriddenDialogView::~SettingsOverriddenDialogView() {
  if (!result_) {
    // The dialog may close without firing any of the [accept | cancel | close]
    // callbacks if e.g. the parent window closes. In this case, notify the
    // controller that the dialog closed without user action.
    controller_->HandleDialogResult(
        SettingsOverriddenDialogController::DialogResult::
            kDialogClosedWithoutUserAction);
  }
}

void SettingsOverriddenDialogView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();

  const gfx::VectorIcon* icon = controller_->GetShowParams().icon;
  if (icon) {
    SetIcon(
        gfx::CreateVectorIcon(*icon,
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE),
                              GetColorProvider()->GetColor(ui::kColorIcon)));
  }
}

void SettingsOverriddenDialogView::Show(gfx::NativeWindow parent) {
  constrained_window::CreateBrowserModalDialogViews(this, parent)->Show();
  controller_->OnDialogShown();
}

void SettingsOverriddenDialogView::NotifyControllerOfResult(
    SettingsOverriddenDialogController::DialogResult result) {
  DCHECK(!result_)
      << "Trying to re-notify controller of result. Previous result: "
      << static_cast<int>(*result_)
      << ", new result: " << static_cast<int>(result);
  result_ = result;
  controller_->HandleDialogResult(result);
}

BEGIN_METADATA(SettingsOverriddenDialogView, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowExtensionSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    Browser* browser) {
  // Note: ownership is taken by the view hierarchy.
  auto* dialog_view = new SettingsOverriddenDialogView(std::move(controller));
  dialog_view->Show(browser->window()->GetNativeWindow());
}

}  // namespace chrome
