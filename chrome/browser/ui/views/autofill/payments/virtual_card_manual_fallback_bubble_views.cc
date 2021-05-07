// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/grid_layout.h"
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

std::unique_ptr<views::MdTextButton> CreateRowItemButton(std::u16string text) {
  auto button = std::make_unique<views::MdTextButton>();
  button->SetText(text);
  button->SetCornerRadius(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, button->GetPreferredSize()));
  button->SetEnabledTextColors(
      views::style::GetColor(*button.get(), views::style::CONTEXT_BUTTON_MD,
                             views::style::STYLE_SECONDARY));
  return button;
}

}  // namespace

VirtualCardManualFallbackBubbleViews::VirtualCardManualFallbackBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    VirtualCardManualFallbackBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller_);
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
}

VirtualCardManualFallbackBubbleViews::~VirtualCardManualFallbackBubbleViews() {
  Hide();
}

void VirtualCardManualFallbackBubbleViews::Hide() {
  CloseBubble();
  if (controller_)
    controller_->OnBubbleClosed(closed_reason_);
  controller_ = nullptr;
}

void VirtualCardManualFallbackBubbleViews::Init() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(0);

  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Adds a row for virtual card number.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(
      CreateRowItemLabel(controller_->GetVirtualCardNumberFieldLabel()));
  layout->AddView(CreateRowItemButton(controller_->GetVirtualCard()->number()));

  // Adds a row for expiration date.
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, 0,
                              views::GridLayout::kFixedSize,
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  layout->AddView(
      CreateRowItemLabel(controller_->GetExpirationDateFieldLabel()));
  auto expiry_row = std::make_unique<views::View>();
  expiry_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(
                      /*vertical=*/0,
                      /*horizontal=*/
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  expiry_row->AddChildView(CreateRowItemButton(
      controller_->GetVirtualCard()->Expiration2DigitMonthAsString()));
  expiry_row->AddChildView(std::make_unique<views::Label>(u"/"));
  // TODO(crbug.com/1196021): Validate this works when the expiration year field
  // is for two-digit numbers
  expiry_row->AddChildView(CreateRowItemButton(
      controller_->GetVirtualCard()->Expiration4DigitYearAsString()));
  layout->AddView(std::move(expiry_row));

  // Adds a row for CVC.
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, 0,
                              views::GridLayout::kFixedSize,
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  layout->AddView(CreateRowItemLabel(controller_->GetCvcFieldLabel()));
  layout->AddView(CreateRowItemButton(controller_->GetCvc()));
}

void VirtualCardManualFallbackBubbleViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(GetWindowTitle()));
}

std::u16string VirtualCardManualFallbackBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetBubbleTitle() : std::u16string();
}

void VirtualCardManualFallbackBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(closed_reason_);
    controller_ = nullptr;
  }
}

void VirtualCardManualFallbackBubbleViews::OnWidgetClosing(
    views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetClosing(widget);
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kAcceptButtonClicked);
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kCancelButtonClicked);
  closed_reason_ = GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
      widget->closed_reason());
}

}  // namespace autofill
