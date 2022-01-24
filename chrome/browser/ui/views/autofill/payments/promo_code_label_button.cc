// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/background.h"

namespace autofill {

// TODO(crbug.com/1248523): Move these to theme constants.
const SkColor kLightModeBackgroundBaseColor = gfx::kGoogleGreen050;
const SkColor kLightModeBackgroundInkDropColor = gfx::kGoogleGreen600;
const SkColor kLightModeTextDefaultColor = gfx::kGoogleGreen800;
const SkColor kLightModeTextHoveredColor = gfx::kGoogleGreen900;
const SkColor kLightModeTextPressedColor = gfx::kGoogleGreen900;
const SkColor kDarkModeBackgroundBaseColor = SkColorSetRGB(0x2C, 0x35, 0x32);
const SkColor kDarkModeBackgroundInkDropColor = gfx::kGoogleGreen300;
const SkColor kDarkModeTextDefaultColor = gfx::kGoogleGreen300;
const SkColor kDarkModeTextHoveredColor = gfx::kGoogleGreen200;
const SkColor kDarkModeTextPressedColor = gfx::kGoogleGreen200;

const int kContentCopyIconSizePx = 24;
const int kFontSizeIncreasePx = 3;
const int kHorizontalPaddingPx = 10;

PromoCodeLabelButton::PromoCodeLabelButton(PressedCallback callback,
                                           const std::u16string& text)
    : views::LabelButton(std::move(callback), text) {}

PromoCodeLabelButton::~PromoCodeLabelButton() = default;

void PromoCodeLabelButton::OnThemeChanged() {
  Button::OnThemeChanged();

  bool use_dark_mode = color_utils::IsDark(GetCascadingBackgroundColor(this));

  SetTextColor(views::Button::STATE_NORMAL, use_dark_mode
                                                ? kDarkModeTextDefaultColor
                                                : kLightModeTextDefaultColor);
  SetTextColor(views::Button::STATE_HOVERED, use_dark_mode
                                                 ? kDarkModeTextHoveredColor
                                                 : kLightModeTextHoveredColor);
  SetTextColor(views::Button::STATE_PRESSED, use_dark_mode
                                                 ? kDarkModeTextPressedColor
                                                 : kLightModeTextPressedColor);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     use_dark_mode ? kDarkModeTextDefaultColor
                                                   : kLightModeTextDefaultColor,
                                     kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     use_dark_mode ? kDarkModeTextHoveredColor
                                                   : kLightModeTextHoveredColor,
                                     kContentCopyIconSizePx));
  SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(vector_icons::kContentCopyIcon,
                                     use_dark_mode ? kDarkModeTextPressedColor
                                                   : kLightModeTextPressedColor,
                                     kContentCopyIconSizePx));
  SetBackground(views::CreateRoundedRectBackground(
      use_dark_mode ? kDarkModeBackgroundBaseColor
                    : kLightModeBackgroundBaseColor,
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium)));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(
      use_dark_mode ? kDarkModeBackgroundInkDropColor
                    : kLightModeBackgroundInkDropColor);
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
