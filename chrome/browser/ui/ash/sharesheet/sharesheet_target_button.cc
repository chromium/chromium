// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/style/typography.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Sizes are in px.
// kButtonTextMaxWidth is button max width without padding.
constexpr int kButtonTextMaxWidth = 76;
constexpr int kButtonMaxLines = 2;
constexpr int kButtonPadding = 8;

}  // namespace

namespace ash {
namespace sharesheet {

// A button that represents a candidate share target.
// Only apps will have |icon| values, while share_actions will have a
// |vector_icon| which is used to generate a |ColorTrackingIconView|. If
// |icon| has a value |vector_icon| should be nullptr and vice versa. There
// should never be a case where both don't have values or both have values.
// It is safe to use |vector_icon| as a raw pointer because it has the same
// lifetime as the |SharesheetService|, which outlives|SharesheetTargetButton|
// as it is a transient UI invoked from the |SharesheetService|.
SharesheetTargetButton::SharesheetTargetButton(
    PressedCallback callback,
    const std::u16string& display_name,
    const std::u16string& secondary_display_name,
    const absl::optional<gfx::ImageSkia> icon,
    const gfx::VectorIcon* vector_icon,
    bool is_dlp_blocked)
    : Button(std::move(callback)), vector_icon_(vector_icon) {
  SetFocusBehavior(View::FocusBehavior::ALWAYS);
  // TODO(crbug.com/1097623) Margins shouldn't be within
  // SharesheetTargetButton as the margins are different in |expanded_view_|.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
      kButtonPadding, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  image_ = AddChildView(std::make_unique<views::ImageView>());
  if (icon.has_value()) {
    image_->SetImage(icon.value());
    vector_icon_ = nullptr;
  }
  image_->SetCanProcessEventsWithinSubtree(false);

  auto label_view = std::make_unique<views::View>();
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));

  auto* label =
      label_view->AddChildView(std::make_unique<views::Label>(display_name));
  label->SetEnabledColorId(cros_tokens::kTextColorSecondary);

  if (chromeos::features::IsJellyEnabled()) {
    bubble_utils::ApplyStyle(label, TypographyToken::kCrosButton1);
  } else {
    label->SetTextContext(CONTEXT_SHARESHEET_BUBBLE_BODY);
    label->SetTextStyle(STYLE_SHARESHEET);
  }
  SetLabelProperties(label);

  std::u16string accessible_name = display_name;
  if (!secondary_display_name.empty() &&
      secondary_display_name != display_name) {
    auto* secondary_label = label_view->AddChildView(
        std::make_unique<views::Label>(secondary_display_name));
    secondary_label->SetEnabledColorId(cros_tokens::kTextColorSecondary);

    if (chromeos::features::IsJellyEnabled()) {
      bubble_utils::ApplyStyle(secondary_label, TypographyToken::kCrosBody2);
    } else {
      secondary_label->SetTextContext(CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY);
      secondary_label->SetTextStyle(STYLE_SHARESHEET);
    }
    SetLabelProperties(secondary_label);
    accessible_name =
        base::StrCat({display_name, u" ", secondary_display_name});
    // As there is a secondary label, don't let the initial label stretch across
    // multiple lines.
    label->SetMultiLine(false);
    secondary_label->SetMultiLine(false);
  } else {
    label->SetMaxLines(kButtonMaxLines);
  }

  AddChildView(std::move(label_view));
  SetAccessibleName(accessible_name);

  if (is_dlp_blocked) {
    SetState(ButtonState::STATE_DISABLED);
  }
}

void SharesheetTargetButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  if (!vector_icon_)
    return;

  // TODO(b/284175205): Convert this to an ImageModel after Jelly launches.
  auto* color_provider = GetColorProvider();
  SkColor icon_color = color_provider->GetColor(
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
          : cros_tokens::kIconColorProminent);
  SkColor circle_color = color_provider->GetColor(
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimaryContainer)
          : cros_tokens::kBgColorElevation1);

  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      *vector_icon_, ::sharesheet::kIconSize / 2, icon_color);
  gfx::ImageSkia circle_icon =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          ::sharesheet::kIconSize / 2, circle_color, icon);

  if (chromeos::features::IsJellyEnabled()) {
    gfx::ShadowValues shadow_values;
    const SkColor shadow_color =
        GetColorProvider()->GetColor(kColorSharesheetTargetButtonIconShadow);
    shadow_values.push_back(
        gfx::ShadowValue(gfx::Vector2d(0, 1), 0, shadow_color));
    shadow_values.push_back(
        gfx::ShadowValue(gfx::Vector2d(0, 1), 2, shadow_color));
    gfx::ImageSkia circle_icon_with_shadow =
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(circle_icon,
                                                            shadow_values);
    image_->SetImage(circle_icon_with_shadow);
  } else {
    image_->SetImage(circle_icon);
  }
}

void SharesheetTargetButton::SetLabelProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetMaximumWidth(kButtonTextMaxWidth);
  label->SetHandlesTooltips(true);
  label->SetTooltipText(label->GetText());
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

BEGIN_METADATA(SharesheetTargetButton, views::Button)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
