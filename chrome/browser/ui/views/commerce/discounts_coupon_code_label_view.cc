// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_coupon_code_label_view.h"

#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"

const int kContentCopyIconSizePx = 20;
const int kInteriorMarginPx = 6;
const int kPromoCodeMaxWidthPx = 154;
const int kPromoCodeLabelLeftMarginPx = 10;
const int kPromoCodeLabelRightMarginPx = 16;

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleCopyButtonElementId);

DiscountsCouponCodeLabelView::DiscountsCouponCodeLabelView(
    const std::u16string& promo_code_text,
    base::RepeatingClosure copy_button_clicked_callback)
    : promo_code_text_(promo_code_text),
      copy_button_clicked_callback_(std::move(copy_button_clicked_callback)) {
  SetInteriorMargin(gfx::Insets::TLBR(kInteriorMarginPx, kInteriorMarginPx,
                                      kInteriorMarginPx, kInteriorMarginPx));

  // TODO(crbug.com/40227597): Remove the view wrappers when the bug is
  // resolved.
  auto* promo_code_label_container =
      AddChildView(std::make_unique<views::View>());
  promo_code_label_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  auto* promo_code_label =
      promo_code_label_container->AddChildView(std::make_unique<views::Label>(
          promo_code_text, views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_2));
  promo_code_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  promo_code_label->SetMaximumWidthSingleLine(kPromoCodeMaxWidthPx);
  promo_code_label_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  promo_code_label_container->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kPromoCodeLabelLeftMarginPx, 0,
                                            kPromoCodeLabelRightMarginPx));

  copy_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DiscountsCouponCodeLabelView::OnCopyButtonClicked,
                          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(IDS_DISCOUNT_CODE_COPY_BUTTON_TEXT)));
  copy_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     ui::kColorIcon, kContentCopyIconSizePx));
  copy_button_->SetStyle(ui::ButtonStyle::kTonal);
  UpdateCopyButtonTooltipsAndAccessibleNames(
      l10n_util::GetStringUTF16(IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP));
  copy_button_->SetProperty(views::kElementIdentifierKey,
                            kDiscountsBubbleCopyButtonElementId);
}

DiscountsCouponCodeLabelView::~DiscountsCouponCodeLabelView() = default;

void DiscountsCouponCodeLabelView::UpdateCopyButtonTooltipsAndAccessibleNames(
    std::u16string tooltip) {
  copy_button_->SetTooltipText(tooltip);
  copy_button_->GetViewAccessibility().SetName(
      base::StrCat({copy_button_->GetText(), u" ", tooltip}));
}

void DiscountsCouponCodeLabelView::OnThemeChanged() {
  views::FlexLayoutView::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(ui::kColorSysNeutralContainer),
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)));
}

gfx::Size DiscountsCouponCodeLabelView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int dialog_inset = views::LayoutProvider::Get()
                               ->GetInsetsMetric(views::INSETS_DIALOG)
                               .left();
  const int dialog_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const int preferred_width = dialog_width - dialog_inset * 2;

  return gfx::Size(
      preferred_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, preferred_width));
}

void DiscountsCouponCodeLabelView::OnCopyButtonClicked() {
  // Copy clicked promo code to clipboard.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(promo_code_text_);

  UpdateCopyButtonTooltipsAndAccessibleNames(l10n_util::GetStringUTF16(
      IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP_CLICKED));

  copy_button_clicked_callback_.Run();
}

BEGIN_METADATA(DiscountsCouponCodeLabelView)
END_METADATA
