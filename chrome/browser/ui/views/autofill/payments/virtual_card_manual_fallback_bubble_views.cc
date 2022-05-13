// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

std::unique_ptr<views::Label> CreateRowItemLabel(std::u16string text) {
  auto label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetMultiLine(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

}  // namespace

VirtualCardManualFallbackBubbleViews::VirtualCardManualFallbackBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    VirtualCardManualFallbackBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller_);
  SetShowIcon(true);
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));
  // Since this bubble provides full card information for filling the credit
  // card form, users may interact with this bubble multiple times. This is to
  // make sure the bubble does not dismiss itself after one interaction. The
  // bubble will instead be dismissed when page navigates or the close button is
  // clicked.
  set_close_on_deactivate(false);
}

VirtualCardManualFallbackBubbleViews::~VirtualCardManualFallbackBubbleViews() {
  Hide();
}

void VirtualCardManualFallbackBubbleViews::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void VirtualCardManualFallbackBubbleViews::Init() {
  auto* const layout_provider = ChromeLayoutProvider::Get();
  const int vertical_padding = layout_provider->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_padding));

  std::u16string explanation = controller_->GetEducationalBodyLabel();
  if (!explanation.empty()) {
    auto* const explanation_label =
        AddChildView(std::make_unique<views::StyledLabel>());
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    explanation_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    explanation_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    explanation_label->SetText(explanation);

    views::StyledLabel::RangeStyleInfo style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &VirtualCardManualFallbackBubbleViews::LearnMoreLinkClicked,
            weak_ptr_factory_.GetWeakPtr()));

    uint32_t offset =
        explanation.length() - controller_->GetLearnMoreLinkText().length();
    explanation_label->AddStyleRange(
        gfx::Range(offset,
                   offset + controller_->GetLearnMoreLinkText().length()),
        style_info);
  }

  auto* card_information_section =
      AddChildView(std::make_unique<views::View>());
  card_information_section
      ->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize)  // Virtual card number.
      .AddPaddingRow(views::TableLayout::kFixedSize, vertical_padding)
      .AddRows(1, views::TableLayout::kFixedSize)  // Expiration date.
      .AddPaddingRow(views::TableLayout::kFixedSize, vertical_padding)
      .AddRows(1, views::TableLayout::kFixedSize)  // Cardholder name.
      .AddPaddingRow(views::TableLayout::kFixedSize, vertical_padding)
      .AddRows(1, views::TableLayout::kFixedSize);  // CVC.

  // Virtual card number.
  card_information_section->AddChildView(
      CreateRowItemLabel(controller_->GetVirtualCardNumberFieldLabel()));
  card_information_section->AddChildView(CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField::kCardNumber));

  // Expiration date.
  card_information_section->AddChildView(
      CreateRowItemLabel(controller_->GetExpirationDateFieldLabel()));
  auto* expiry_row =
      card_information_section->AddChildView(std::make_unique<views::View>());
  expiry_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  expiry_row->AddChildView(CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField::kExpirationMonth));
  expiry_row->AddChildView(std::make_unique<views::Label>(u"/"));
  // TODO(crbug.com/1196021): Validate this works when the expiration year field
  // is for two-digit numbers
  expiry_row->AddChildView(CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField::kExpirationYear));

  // Cardholder name.
  card_information_section->AddChildView(
      CreateRowItemLabel(controller_->GetCardholderNameFieldLabel()));
  card_information_section->AddChildView(CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField::kCardholderName));

  // CVC.
  card_information_section->AddChildView(
      CreateRowItemLabel(controller_->GetCvcFieldLabel()));
  card_information_section->AddChildView(
      CreateRowItemButtonForField(VirtualCardManualFallbackBubbleField::kCvc));
  UpdateButtonTooltipsAndAccessibleNames();
}

ui::ImageModel VirtualCardManualFallbackBubbleViews::GetWindowIcon() {
  // Fall back to network icon if no specific icon is provided.
  // TODO(crbug.com/1218628): Fallback logic might be put inside
  // BrowserAutofillManager or PDM. Remove GetVirtualCard() afterwards.
  if (controller_->GetBubbleTitleIcon().IsEmpty()) {
    gfx::Image card_image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            CreditCard::IconResourceId(
                controller_->GetVirtualCard()->network()));
    return ui::ImageModel::FromImage(card_image);
  }
  return ui::ImageModel::FromImage(controller_->GetBubbleTitleIcon());
}

std::u16string VirtualCardManualFallbackBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetBubbleTitleText() : std::u16string();
}

void VirtualCardManualFallbackBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void VirtualCardManualFallbackBubbleViews::OnWidgetDestroying(
    views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  if (!widget->IsClosed())
    return;
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kAcceptButtonClicked);
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kCancelButtonClicked);
}

std::unique_ptr<views::MdTextButton>
VirtualCardManualFallbackBubbleViews::CreateRowItemButtonForField(
    VirtualCardManualFallbackBubbleField field) {
  std::u16string text = controller_->GetValueForField(field);
  auto button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&VirtualCardManualFallbackBubbleViews::OnFieldClicked,
                          weak_ptr_factory_.GetWeakPtr(), field),
      text, views::style::CONTEXT_BUTTON);
  button->SetCornerRadius(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, button->GetPreferredSize()));
  fields_to_buttons_map_[field] = button.get();
  return button;
}

void VirtualCardManualFallbackBubbleViews::OnFieldClicked(
    VirtualCardManualFallbackBubbleField field) {
  controller_->OnFieldClicked(field);
  UpdateButtonTooltipsAndAccessibleNames();
}

void VirtualCardManualFallbackBubbleViews::
    UpdateButtonTooltipsAndAccessibleNames() {
  for (auto& pair : fields_to_buttons_map_) {
    std::u16string tooltip = controller_->GetFieldButtonTooltip(pair.first);
    pair.second->SetTooltipText(tooltip);
    pair.second->SetAccessibleName(
        base::StrCat({pair.second->GetText(), u" ", tooltip}));
  }
}

void VirtualCardManualFallbackBubbleViews::LearnMoreLinkClicked() {
  if (controller_) {
    controller_->OnLinkClicked(
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }
}

}  // namespace autofill
