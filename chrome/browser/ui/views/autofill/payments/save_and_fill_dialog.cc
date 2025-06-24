// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace autofill {

SaveAndFillDialog::SaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller)
    : controller_(controller) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_->GetAcceptButtonText());
  SetShowCloseButton(false);
  InitViews();
}

SaveAndFillDialog::~SaveAndFillDialog() = default;

void SaveAndFillDialog::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }

  if (controller_->IsUploadSaveAndFill()) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  } else {
    auto title_view = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view->SetMultiLine(true);
    GetBubbleFrameView()->SetTitleView(std::move(title_view));
  }
}

void SaveAndFillDialog::RemovedFromWidget() {
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

std::u16string SaveAndFillDialog::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveAndFillDialog::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  if (sender == &card_number_data_.GetInputTextField()) {
    card_number_data_.SetErrorState(
        /*is_valid_input=*/controller_->IsValidCreditCardNumber(new_contents),
        /*error_message=*/controller_->GetInvalidCardNumberErrorMessage());
  } else if (sender == &cvc_data_.GetInputTextField()) {
    cvc_data_.SetErrorState(
        /*is_valid_input=*/controller_->IsValidCvc(new_contents),
        /*error_message=*/controller_->GetInvalidCvcErrorMessage());
  } else if (sender == &name_on_card_data_.GetInputTextField()) {
    name_on_card_data_.SetErrorState(
        /*is_valid_input=*/controller_->IsValidNameOnCard(new_contents),
        /*error_message=*/controller_->GetInvalidNameOnCardErrorMessage());
  }
}

void SaveAndFillDialog::OnDidChangeFocus(views::View* before,
                                         views::View* now) {
  if (before == &card_number_data_.GetInputTextField()) {
    card_number_data_.GetInputTextField().SetText(
        GetFormattedCardNumberForDisplay(
            card_number_data_.GetInputTextField().GetText()));
  }
}

void SaveAndFillDialog::InitViews() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  AddChildView(views::Builder<views::Label>()
                   .SetText(controller_->GetExplanatoryMessage())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                   .Build());

  card_number_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetCardNumberLabel(),
      /*error_message=*/controller_->GetInvalidCardNumberErrorMessage());
  card_number_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  card_number_data_.GetInputTextField().SetController(this);
  AddChildView(std::move(card_number_data_.container));

  expiration_date_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetExpirationDateLabel(),
      /*error_message=*/std::u16string());
  expiration_date_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_DATE);
  expiration_date_data_.GetInputTextField().SetController(this);
  expiration_date_data_.GetInputTextField().SetPlaceholderText(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPIRATION_DATE_PLACEHOLDER));
  expiration_date_data_.GetInputTextField().SetDefaultWidthInChars(18);

  cvc_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetCvcLabel(),
      /*error_message=*/std::u16string());
  cvc_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  cvc_data_.GetInputTextField().SetController(this);
  cvc_data_.GetInputTextField().SetPlaceholderText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CVC_PLACEHOLDER));
  cvc_data_.GetInputTextField().SetDefaultWidthInChars(18);

  // Create the horizontal row for expiration date, cvc, and icon.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
          .AddChild(views::Builder<views::View>(
              std::move(expiration_date_data_.container)))
          .AddChild(views::Builder<views::View>(std::move(cvc_data_.container)))
          .AddChild(views::Builder<views::ImageView>().SetImage(
              ui::ImageModel::FromImage(
                  ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                      IDR_CREDIT_CARD_CVC_HINT_BACK))))
          .Build());

  name_on_card_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetNameOnCardLabel(),
      /*error_message=*/controller_->GetInvalidNameOnCardErrorMessage());
  name_on_card_data_.GetInputTextField().SetController(this);
  AddChildView(std::move(name_on_card_data_.container));
}

}  // namespace autofill
