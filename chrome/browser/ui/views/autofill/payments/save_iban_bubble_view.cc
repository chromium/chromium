// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

// Creates eye icon view to toggle between the masked or revealed IBAN value
// on click.
std::unique_ptr<views::ToggleImageButton> CreateIbanMaskingToggle(
    views::Button::PressedCallback callback) {
  auto button = std::make_unique<views::ToggleImageButton>(std::move(callback));
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_IBAN_VALUE_SHOW_VALUE));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_IBAN_VALUE_HIDE_VALUE));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetToggled(false);
  return button;
}

}  // namespace

SaveIbanBubbleView::SaveIbanBubbleView(views::View* anchor_view,
                                       content::WebContents* web_contents,
                                       IbanBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetCancelCallback(base::BindOnce(&SaveIbanBubbleView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&SaveIbanBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SaveIbanBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogButtonsForTesting();  // IN-TEST
}

void SaveIbanBubbleView::ToggleIbanValueMasking() {
  const bool is_value_masked = iban_value_masking_button_->GetToggled();
  iban_value_masking_button_->SetToggled(!is_value_masked);
  iban_value_->SetText(GetIbanIdentifierString(is_value_masked));
}

void SaveIbanBubbleView::Hide() {
  CloseBubble();

  // If `controller_` is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out `controller_`'s reference to `this`. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveIbanBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD_SECURELY),
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD_SECURELY_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));

  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::PRODUCT_LOGO));
}

std::u16string SaveIbanBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveIbanBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

SaveIbanBubbleView::~SaveIbanBubbleView() = default;

void SaveIbanBubbleView::CreateMainContentView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();

  auto* iban_view = AddChildView(std::make_unique<views::BoxLayoutView>());
  iban_view->SetID(DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  views::TableLayout* layout =
      iban_view->SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(
          views::TableLayout::kFixedSize,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      // Add a row for IBAN label and the value of IBAN.
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     ChromeLayoutProvider::Get()->GetDistanceMetric(
                         DISTANCE_CONTROL_LIST_VERTICAL))
      // Add a row for nickname label and the input text field.
      .AddRows(1, views::TableLayout::kFixedSize);

  iban_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  auto* iban_value_view =
      iban_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  iban_value_ = iban_value_view->AddChildView(std::make_unique<views::Label>(
      GetIbanIdentifierString(/*is_value_masked=*/true),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  iban_value_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  iban_value_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  iban_value_masking_button_ =
      iban_value_view->AddChildView(CreateIbanMaskingToggle(
          base::BindRepeating(&SaveIbanBubbleView::ToggleIbanValueMasking,
                              base::Unretained(this))));
  views::SetImageFromVectorIconWithColorId(iban_value_masking_button_,
                                           views::kEyeIcon, ui::kColorIcon,
                                           ui::kColorIconDisabled);
  views::SetToggledImageFromVectorIconWithColorId(
      iban_value_masking_button_, views::kEyeCrossedIcon, ui::kColorIcon,
      ui::kColorIconDisabled);

  iban_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  nickname_textfield_ =
      iban_view->AddChildView(std::make_unique<views::Textfield>());
  nickname_textfield_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME));
  nickname_textfield_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
  nickname_textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PLACEHOLDER));
}

void SaveIbanBubbleView::AssignIdsToDialogButtonsForTesting() {
  auto* ok_button = GetOkButton();
  if (ok_button) {
    ok_button->SetID(DialogViewId::OK_BUTTON);
  }
  auto* cancel_button = GetCancelButton();
  if (cancel_button) {
    cancel_button->SetID(DialogViewId::CANCEL_BUTTON);
  }

  DCHECK(iban_value_masking_button_);
  iban_value_masking_button_->SetID(
      DialogViewId::TOGGLE_IBAN_VALUE_MASKING_BUTTON);

  DCHECK(iban_value_);
  iban_value_->SetID(DialogViewId::IBAN_VALUE_LABEL);

  if (nickname_textfield_) {
    nickname_textfield_->SetID(DialogViewId::NICKNAME_TEXTFIELD);
  }
}

void SaveIbanBubbleView::OnDialogAccepted() {
  if (controller_) {
    DCHECK(nickname_textfield_);
    controller_->OnAcceptButton(nickname_textfield_->GetText());
  }
}

void SaveIbanBubbleView::OnDialogCancelled() {
  if (controller_) {
    controller_->OnCancelButton();
  }
}

void SaveIbanBubbleView::Init() {
  CreateMainContentView();
}

std::u16string SaveIbanBubbleView::GetIbanIdentifierString(
    bool is_value_masked) const {
  return controller_->GetIBAN().GetIdentifierStringForAutofillDisplay(
      is_value_masked);
}

}  // namespace autofill
