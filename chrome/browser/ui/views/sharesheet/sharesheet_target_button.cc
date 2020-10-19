// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet/sharesheet_target_button.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

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

constexpr char kButtonLabelFont[] = "Roboto, Medium, 14px";
constexpr char kButtonSecondaryLabelFont[] = "Roboto, Regular, 13px";

constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;
constexpr SkColor kShareTargetSecondaryTitleColor = gfx::kGoogleGrey600;

}  // namespace

// A button that represents a candidate share target.
SharesheetTargetButton::SharesheetTargetButton(
    views::ButtonListener* listener,
    const base::string16& display_name,
    const base::string16& secondary_display_name,
    const gfx::ImageSkia* icon)
    : Button(listener) {
  // TODO(crbug.com/1097623) Margins shouldn't be within
  // SharesheetTargetButton as the margins are different in |expanded_view_|.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
      kButtonPadding, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* image = AddChildView(std::make_unique<views::ImageView>());
  image->SetCanProcessEventsWithinSubtree(false);

  if (!icon->isNull()) {
    image->SetImage(icon);
  }

  auto label_view = std::make_unique<views::View>();
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));

  auto* label =
      label_view->AddChildView(std::make_unique<views::Label>(display_name));
  label->SetFontList(gfx::FontList(kButtonLabelFont));
  label->SetEnabledColor(kShareTargetTitleColor);
  SetLabelProperties(label);

  base::string16 accessible_name = display_name;
  if (secondary_display_name != base::string16() &&
      secondary_display_name != display_name) {
    auto* secondary_label = label_view->AddChildView(
        std::make_unique<views::Label>(secondary_display_name));
    secondary_label->SetFontList(gfx::FontList(kButtonSecondaryLabelFont));
    secondary_label->SetEnabledColor(kShareTargetSecondaryTitleColor);
    SetLabelProperties(secondary_label);
    accessible_name = base::StrCat(
        {display_name, base::ASCIIToUTF16(" "), secondary_display_name});
    // As there is a secondary label, don't let the initial label stretch across
    // multiple lines.
    label->SetMultiLine(false);
    secondary_label->SetMultiLine(false);
  } else {
    label->SetMaxLines(kButtonMaxLines);
  }

  AddChildView(std::move(label_view));
  SetAccessibleName(accessible_name);

  SetFocusForPlatform();
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
