// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_confirmation_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "components/exo/wm_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

namespace {

class ResizeConfirmationDialogDelegate : public views::DialogDelegate {
 public:
  ResizeConfirmationDialogDelegate(ResizeConfirmationCallback callback,
                                   views::Checkbox* do_not_ask_checkbox)
      : callback_(std::move(callback)),
        do_not_ask_checkbox_(do_not_ask_checkbox) {}
  ResizeConfirmationDialogDelegate(const ResizeConfirmationDialogDelegate&) =
      delete;
  ResizeConfirmationDialogDelegate& operator=(
      const ResizeConfirmationDialogDelegate&) = delete;
  ~ResizeConfirmationDialogDelegate() override = default;

  void RunCallback(bool accept) {
    DCHECK(callback_);
    std::move(callback_).Run(accept, do_not_ask_checkbox_->GetChecked());
  }

 private:
  ResizeConfirmationCallback callback_;
  const views::Checkbox* do_not_ask_checkbox_;
};

std::unique_ptr<views::DialogDelegate> MakeDialogDelegate(
    ResizeConfirmationCallback callback) {
  // Setup contents.
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  auto contents = std::make_unique<views::BoxLayoutView>();
  contents->SetOrientation(views::BoxLayout::Orientation::kVertical);
  contents->SetInsideBorderInsets(
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
  contents->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  auto* message_label = contents->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_BODY),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetMultiLine(true);

  auto* checkbox = contents->AddChildView(
      std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
          IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_DONT_ASK_ME)));

  // Setup delegate.
  auto delegate = std::make_unique<ResizeConfirmationDialogDelegate>(
      std::move(callback), checkbox);
  delegate->SetContentsView(std::move(contents));
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);
  delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_TITLE));
  delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_ACCEPT));
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Safe to set "Unretained" as |delegate| is owned by widget, so keeps
  // alive within the life-time of the widget.
  delegate->SetAcceptCallback(
      base::BindOnce(&ResizeConfirmationDialogDelegate::RunCallback,
                     base::Unretained(delegate.get()), /*accept=*/true));
  delegate->SetCancelCallback(
      base::BindOnce(&ResizeConfirmationDialogDelegate::RunCallback,
                     base::Unretained(delegate.get()), /*accept=*/false));

  return delegate;
}

}  // namespace

views::Widget* ShowResizeConfirmationDialog(
    aura::Window* parent,
    ResizeConfirmationCallback callback) {
  // TOOD(b/183664767): Switch dialog to use exo's overlay.
  auto* widget = views::DialogDelegate::CreateDialogWidget(
      MakeDialogDelegate(std::move(callback)),
      exo::WMHelper::GetInstance()->GetRootWindowForNewWindows(), parent);
  widget->Show();
  return widget;
}

}  // namespace arc
