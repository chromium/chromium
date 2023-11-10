// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace plus_addresses {

namespace {
const float kDescriptionWidthPercent = 0.8;
const int kPlusAddressLabelTopMargin = 10;
}  // namespace

PlusAddressCreationDialogDelegate::PlusAddressCreationDialogDelegate(
    base::WeakPtr<PlusAddressCreationController> controller,
    const std::string& primary_email_address)
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE) {
  // This delegate is owned & deleted by the PlusAddressCreationController.
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](base::WeakPtr<PlusAddressCreationController> controller) {
        controller->OnDialogDestroyed();
      },
      controller));
  SetModalType(ui::MODAL_TYPE_CHILD);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  std::unique_ptr<views::View> primary_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  // Create hero image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  primary_view->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromImageSkia(
              // TODO(crbug.com/1467623) - Replace this placeholder image.
              *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_CONSENTED)))
          .Build());

  // Add title view.
  primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetTextContext(views::style::STYLE_PRIMARY)
          .SetText(l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetDefaultTextStyle(views::style::STYLE_BODY_1_BOLD)
          .Build());

  views::StyledLabel* description_paragraph = primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .Build());

  // Split the difference on both sides of the description.
  int horizontal_margin = (1 - kDescriptionWidthPercent) *
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) /
                          2;
  description_paragraph->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, horizontal_margin, 0, horizontal_margin));

  // Set the description text & update the styling.
  std::vector<size_t> description_offsets;
  // Prepend the settings link text with a newline to render it on one line.
  std::u16string settings_text =
      base::StrCat({u"\n", l10n_util::GetStringUTF16(
                               IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_LINK_TEXT)});
  description_paragraph->SetText(l10n_util::GetStringFUTF16(
      IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_START, {settings_text},
      &description_offsets));

  gfx::Range settings_text_range(
      description_offsets[0], description_offsets[0] + settings_text.length());
  views::StyledLabel::RangeStyleInfo settings_text_style;
  settings_text_style.text_style = views::style::TextStyle::STYLE_LINK;
  description_paragraph->AddStyleRange(settings_text_range,
                                       settings_text_style);

  // Add the primary email address separately to avoid width constriction.
  views::StyledLabel* primary_email_address_view = primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .Build());

  // Set the primary email address  & update the styling.
  std::vector<size_t> email_address_offsets;
  std::u16string u16_primary_email_address =
      base::UTF8ToUTF16(primary_email_address);
  primary_email_address_view->SetText(l10n_util::GetStringFUTF16(
      IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_END,
      {u16_primary_email_address}, &email_address_offsets));

  views::StyledLabel::RangeStyleInfo email_address_style;
  email_address_style.text_style = views::style::TextStyle::STYLE_EMPHASIZED;
  primary_email_address_view->AddStyleRange(
      gfx::Range(email_address_offsets[0],
                 email_address_offsets[0] + u16_primary_email_address.length()),
      email_address_style);

  // Create a bubble for the plus address to be displayed in.
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kRectangleRadius =
      provider->GetCornerRadiusMetric(views::ShapeContextTokens::kDialogRadius);

  std::unique_ptr<views::Background> background =
      views::CreateThemedRoundedRectBackground(
          // TODO(crbug.com/1467623) - Replace with color from the mocks.
          ui::kColorSubtleEmphasisBackground, kRectangleRadius);
  plus_address_label_ = primary_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetBackground(std::move(background))
          .Build());
  plus_address_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPlusAddressLabelTopMargin, 0, 0, 0));
  plus_address_label_->SetSelectable(true);
  plus_address_label_->SetLineHeight(2 * plus_address_label_->GetLineHeight());

  // Initialize buttons.
  SetDefaultButton(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT));
  SetButtonEnabled(ui::DialogButton::DIALOG_BUTTON_OK, false);
  SetAcceptCallback(
      base::BindOnce(&PlusAddressCreationController::OnConfirmed, controller));
  SetCancelCallback(
      base::BindOnce(&PlusAddressCreationController::OnCanceled, controller));
  SetCloseCallback(
      base::BindOnce(&PlusAddressCreationController::OnCanceled, controller));

  SetContentsView(std::move(primary_view));
}

PlusAddressCreationDialogDelegate::~PlusAddressCreationDialogDelegate() {
  plus_address_label_ = nullptr;
}

bool PlusAddressCreationDialogDelegate::ShouldShowCloseButton() const {
  return true;
}

void PlusAddressCreationDialogDelegate::OnModalReadyForUse(
    const std::string& plus_address) {
  if (plus_address_label_) {
    plus_address_label_->SetText(base::UTF8ToUTF16(plus_address));
  }
  SetButtonEnabled(ui::DialogButton::DIALOG_BUTTON_OK, true);
}

void PlusAddressCreationDialogDelegate::OnRequestError() {
  if (plus_address_label_) {
    plus_address_label_->SetText(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_ERROR_MESSAGE));
  }
  SetButtonEnabled(ui::DialogButton::DIALOG_BUTTON_OK, false);
}

}  // namespace plus_addresses
