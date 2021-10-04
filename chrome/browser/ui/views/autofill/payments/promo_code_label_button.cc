// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace autofill {

// TODO(crbug.com/1248523): Add in hovered and pressed background colors
//     specified by Chrome UX, including dark mode colors when UX provides them.
const SkColor kBackgroundColor = gfx::kGoogleGreen050;
const SkColor kTextColor = gfx::kGoogleGreen800;

const int kContentCopyIconSizePx = 24;
const int kFontSizeIncreasePx = 3;
const int kHorizontalPaddingPx = 10;

PromoCodeLabelButton::PromoCodeLabelButton(PressedCallback callback,
                                           const std::u16string& text)
    : views::LabelButton(std::move(callback), text) {
  SetTextColor(views::Button::STATE_NORMAL, kTextColor);
  SetTextColor(views::Button::STATE_HOVERED, kTextColor);
  SetTextColor(views::Button::STATE_PRESSED, kTextColor);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon, kTextColor,
                                     kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon, kTextColor,
                                     kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon, kTextColor,
                                     kContentCopyIconSizePx));
  SetBackground(views::CreateRoundedRectBackground(
      kBackgroundColor, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                            views::Emphasis::kMedium)));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      kFontSizeIncreasePx, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  gfx::Insets insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_LABEL_BUTTON);
  insets.set_left(kHorizontalPaddingPx);
  insets.set_right(kHorizontalPaddingPx);
  SetBorder(views::CreateEmptyBorder(insets));
}

PromoCodeLabelButton::~PromoCodeLabelButton() = default;

BEGIN_METADATA(PromoCodeLabelButton, views::LabelButton)
END_METADATA

}  // namespace autofill
