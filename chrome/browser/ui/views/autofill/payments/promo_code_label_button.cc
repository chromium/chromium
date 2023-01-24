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

const int kContentCopyIconSizePx = 24;
const int kFontSizeIncreasePx = 3;
const int kHorizontalPaddingPx = 10;

PromoCodeLabelButton::PromoCodeLabelButton(PressedCallback callback,
                                           const std::u16string& text)
    : views::LabelButton(std::move(callback), text) {}

PromoCodeLabelButton::~PromoCodeLabelButton() = default;

void PromoCodeLabelButton::OnThemeChanged() {
  Button::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  SetTextColor(views::Button::STATE_NORMAL,
               color_provider->GetColor(kColorPaymentsPromoCodeForeground));
  SetTextColor(
      views::Button::STATE_HOVERED,
      color_provider->GetColor(kColorPaymentsPromoCodeForegroundHovered));
  SetTextColor(
      views::Button::STATE_PRESSED,
      color_provider->GetColor(kColorPaymentsPromoCodeForegroundPressed));
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    vector_icons::kContentCopyIcon,
                    kColorPaymentsPromoCodeForeground, kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     kColorPaymentsPromoCodeForegroundHovered,
                                     kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     kColorPaymentsPromoCodeForegroundPressed,
                                     kContentCopyIconSizePx));
  SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorPaymentsPromoCodeBackground),
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium)));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(
      color_provider->GetColor(kColorPaymentsPromoCodeInkDrop));
  SetHasInkDropActionOnClick(true);

  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      kFontSizeIncreasePx, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  gfx::Insets insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_LABEL_BUTTON);
  insets.set_left(kHorizontalPaddingPx);
  insets.set_right(kHorizontalPaddingPx);
  SetBorder(views::CreateEmptyBorder(insets));
}

BEGIN_METADATA(PromoCodeLabelButton, views::LabelButton)
END_METADATA

}  // namespace autofill
