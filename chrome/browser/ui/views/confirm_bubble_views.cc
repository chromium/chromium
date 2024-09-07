// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include <utility>

#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

ConfirmBubbleViews::ConfirmBubbleViews(
    std::unique_ptr<ConfirmBubbleModel> model)
    : model_(std::move(model)) {
  SetModalType(ui::mojom::ModalType::kWindow);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 model_->GetButtonLabel(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 model_->GetButtonLabel(ui::mojom::DialogButton::kCancel));
  SetAcceptCallback(base::BindOnce(&ConfirmBubbleModel::Accept,
                                   base::Unretained(model_.get())));
  SetCancelCallback(base::BindOnce(&ConfirmBubbleModel::Cancel,
                                   base::Unretained(model_.get())));
  views::ImageButton* help_button =
      SetExtraView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              [](ConfirmBubbleViews* bubble) {
                bubble->model_->OpenHelpPage();
                bubble->GetWidget()->Close();
              },
              base::Unretained(this)),
          vector_icons::kHelpOutlineIcon));
  help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetLayoutManager(std::make_unique<views::BoxLayout>());

  label_ = AddChildView(std::make_unique<views::Label>(
      model_->GetMessageText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  DCHECK(!label_->GetText().empty());
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  // Use a fixed maximum message width, so longer messages will wrap.
  label_->SetMaximumWidth(400);
}

ConfirmBubbleViews::~ConfirmBubbleViews() {
}

std::u16string ConfirmBubbleViews::GetWindowTitle() const {
  return model_->GetTitle();
}

bool ConfirmBubbleViews::ShouldShowCloseButton() const {
  return false;
}

void ConfirmBubbleViews::OnWidgetInitialized() {
  GetWidget()->GetRootView()->GetViewAccessibility().SetDescription(*label_);
}

BEGIN_METADATA(ConfirmBubbleViews)
END_METADATA

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       std::unique_ptr<ConfirmBubbleModel> model) {
  constrained_window::CreateBrowserModalDialogViews(
      new ConfirmBubbleViews(std::move(model)), window)
      ->Show();
}

}  // namespace chrome
