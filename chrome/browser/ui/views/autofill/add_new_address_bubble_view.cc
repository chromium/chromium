// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/add_new_address_bubble_view.h"

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AddNewAddressBubbleView::AddNewAddressBubbleView(
    std::unique_ptr<AddNewAddressBubbleController> controller,
    views::View* anchor_view,
    content::WebContents* web_contents)
    : AddressBubbleBaseView(anchor_view, web_contents),
      controller_(std::move(controller)) {
  SetAcceptCallback(
      base::BindOnce(&AddNewAddressBubbleController::OnAddButtonClicked,
                     base::Unretained(controller_.get())));
  SetCancelCallback(
      base::BindOnce(&AddNewAddressBubbleController::OnUserDecision,
                     base::Unretained(controller_.get()),
                     AutofillClient::AddressPromptUserDecision::kDeclined));

  SetTitle(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_PROMPT_TITLE));

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_ADD_NEW_ADDRESS_DIALOG_OK_BUTTON_LABEL));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  AddChildView(views::Builder<views::Label>()
                   .SetText(controller_->GetBodyText())
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .SetMultiLine(true)
                   .Build());

  std::u16string footer_message = controller_->GetFooterMessage();
  if (!footer_message.empty()) {
    SetFootnoteView(
        views::Builder<views::Label>()
            .SetText(footer_message)
            .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }
}

AddNewAddressBubbleView::~AddNewAddressBubbleView() = default;

bool AddNewAddressBubbleView::ShouldShowCloseButton() const {
  return true;
}

void AddNewAddressBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void AddNewAddressBubbleView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_) {
    controller_->OnBubbleClosed();
  }

  controller_ = nullptr;
}

void AddNewAddressBubbleView::AddedToWidget() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  frame_view->SetProperty(views::kElementIdentifierKey, kTopViewId);
  frame_view->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AddNewAddressBubbleView, kTopViewId);

}  // namespace autofill
