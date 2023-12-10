// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/promo_code_label_view.h"

#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace autofill {
const int kContentCopyIconSizePx = 20;
const int kInteriorMarginPx = 6;
const int kPromoCodeMaxWidthPx = 154;
const int kPromoCodeLabelLeftMarginPx = 10;
const int kPromoCodeLabelRightMarginPx = 16;

PromoCodeLabelView::PromoCodeLabelView(
    gfx::Size& preferred_size,
    const std::u16string& promo_code_text,
    views::Button::PressedCallback copy_button_pressed_callback) {
  SetPreferredSize(preferred_size);
  SetInteriorMargin(gfx::Insets::TLBR(kInteriorMarginPx, kInteriorMarginPx,
                                      kInteriorMarginPx, kInteriorMarginPx));

  // TODO(crbug.com/1331844): Remove the view wrappers when the bug is resolved.
  auto* promo_code_label_container =
      AddChildView(std::make_unique<views::View>());
  promo_code_label_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  promo_code_label_ =
      promo_code_label_container->AddChildView(std::make_unique<views::Label>(
          promo_code_text, views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_2));
  promo_code_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  promo_code_label_->SetMaximumWidthSingleLine(kPromoCodeMaxWidthPx);
  promo_code_label_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  promo_code_label_container->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kPromoCodeLabelLeftMarginPx, 0,
                                            kPromoCodeLabelRightMarginPx));

  copy_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      std::move(copy_button_pressed_callback),
      l10n_util::GetStringUTF16(IDS_DISCOUNT_CODE_COPY_BUTTON_TEXT)));
  copy_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     ui::kColorIcon, kContentCopyIconSizePx));
  copy_button_->SetStyle(ui::ButtonStyle::kTonal);
}

PromoCodeLabelView::~PromoCodeLabelView() = default;

void PromoCodeLabelView::UpdateCopyButtonTooltipsAndAccessibleNames(
    std::u16string& tooltip) {
  copy_button_->SetTooltipText(tooltip);
  copy_button_->SetAccessibleName(
      base::StrCat({copy_button_->GetText(), u" ", tooltip}));
}

void PromoCodeLabelView::OnThemeChanged() {
  views::FlexLayoutView::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(ui::kColorSysNeutralContainer),
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)));
}

raw_ptr<views::LabelButton> PromoCodeLabelView::GetCopyButtonForTesting() {
  return copy_button_;
}

const std::u16string& PromoCodeLabelView::GetPromoCodeLabelTextForTesting()
    const {
  return promo_code_label_->GetText();
}

BEGIN_METADATA(PromoCodeLabelView, views::FlexLayoutView)
END_METADATA

}  // namespace autofill
