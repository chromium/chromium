// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"

#include <memory>

#include "ash/public/cpp/ash_typography.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

// Sizes are in px.

// kButtonWidth = 76px width + 2*8px for padding on left and right
constexpr int kButtonWidth = 92;
// kButtonHeight = 88px height + 2*8px for padding on top and bottom.
constexpr int kButtonHeight = 104;
// kButtonTextMaxWidth is button max width without padding.
constexpr int kButtonTextMaxWidth = 76;
constexpr int kButtonLineHeight = 20;
constexpr int kButtonMaxLines = 2;
constexpr int kButtonPadding = 8;

constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;
constexpr SkColor kShareTargetSecondaryTitleColor = gfx::kGoogleGrey600;

std::unique_ptr<views::ImageView> CreateImageView(
    const base::Optional<gfx::ImageSkia> icon,
    const gfx::VectorIcon* vector_icon) {
  if (icon.has_value()) {
    auto image = std::make_unique<views::ImageView>();
    image->SetImage(icon.value());
    return image;
  } else if (vector_icon != nullptr) {
    return std::make_unique<views::ColorTrackingIconView>(
        *vector_icon, sharesheet::kIconSize);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

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
    const base::Optional<gfx::ImageSkia> icon,
    const gfx::VectorIcon* vector_icon)
    : Button(std::move(callback)) {
  // TODO(crbug.com/1097623) Margins shouldn't be within
  // SharesheetTargetButton as the margins are different in |expanded_view_|.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
      kButtonPadding, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* image = AddChildView(CreateImageView(icon, vector_icon));
  image->SetCanProcessEventsWithinSubtree(false);

  auto label_view = std::make_unique<views::View>();
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));

  auto* label = label_view->AddChildView(std::make_unique<views::Label>(
      display_name, ash::CONTEXT_SHARESHEET_BUBBLE_BODY,
      ash::STYLE_SHARESHEET));
  label->SetEnabledColor(kShareTargetTitleColor);
  SetLabelProperties(label);

  std::u16string accessible_name = display_name;
  if (secondary_display_name != std::u16string() &&
      secondary_display_name != display_name) {
    auto* secondary_label =
        label_view->AddChildView(std::make_unique<views::Label>(
            secondary_display_name,
            ash::CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY,
            ash::STYLE_SHARESHEET));
    secondary_label->SetEnabledColor(kShareTargetSecondaryTitleColor);
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
}

void SharesheetTargetButton::SetLabelProperties(views::Label* label) {
  label->SetLineHeight(kButtonLineHeight);
  label->SetMultiLine(true);
  label->SetMaximumWidth(kButtonTextMaxWidth);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHandlesTooltips(true);
  label->SetTooltipText(label->GetText());
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

// Button is 76px width x 88px height + 8px padding along all sides.
gfx::Size SharesheetTargetButton::CalculatePreferredSize() const {
  return gfx::Size(kButtonWidth, kButtonHeight);
}

BEGIN_METADATA(SharesheetTargetButton, views::Button)
END_METADATA
