// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"

namespace autofill {

namespace {

constexpr int kContentCopyIconSizePx = 24;
constexpr int kFontSizeIncreasePx = 3;
constexpr int kHorizontalPaddingPx = 10;

}  // namespace

PromoCodeLabelButton::PromoCodeLabelButton(PressedCallback callback,
                                           const std::u16string& text)
    : views::LabelButton(std::move(callback), text) {
  SetTextColorId(ButtonState::STATE_NORMAL, kColorPaymentsPromoCodeForeground);
  SetTextColorId(ButtonState::STATE_HOVERED,
                 kColorPaymentsPromoCodeForegroundHovered);
  SetTextColorId(ButtonState::STATE_PRESSED,
                 kColorPaymentsPromoCodeForegroundPressed);
  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    vector_icons::kContentCopyIcon,
                    kColorPaymentsPromoCodeForeground, kContentCopyIconSizePx));
  SetImageModel(
      ButtonState::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     kColorPaymentsPromoCodeForegroundHovered,
                                     kContentCopyIconSizePx));
  SetImageModel(
      ButtonState::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     kColorPaymentsPromoCodeForegroundPressed,
                                     kContentCopyIconSizePx));
  SetBackground(views::CreateThemedRoundedRectBackground(
      kColorPaymentsPromoCodeBackground,
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium)));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColorId(kColorPaymentsPromoCodeInkDrop);
  SetHasInkDropActionOnClick(true);

  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      kFontSizeIncreasePx, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()
          ->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .set_left_right(kHorizontalPaddingPx, kHorizontalPaddingPx)));
}

PromoCodeLabelButton::~PromoCodeLabelButton() = default;

BEGIN_METADATA(PromoCodeLabelButton)
END_METADATA

}  // namespace autofill
