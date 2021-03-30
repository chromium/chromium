// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"

#include "ash/public/cpp/ash_typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

// Sizes are in px.
constexpr int kDefaultBubbleWidth = 416;
constexpr int kCaretIconSize = 20;
constexpr int kHeight = 32;
constexpr int kLineHeight = 20;
constexpr int kBetweenChildSpacing = 8;
constexpr int kMarginSpacing = 24;

constexpr SkColor kLabelColor = gfx::kGoogleBlue600;

}  // namespace

SharesheetExpandButton::SharesheetExpandButton(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(6, 16, 6, 16),
      kBetweenChildSpacing, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<views::ImageView>());

  label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), ash::CONTEXT_SHARESHEET_BUBBLE_BODY,
      ash::STYLE_SHARESHEET));
  label_->SetLineHeight(kLineHeight);
  label_->SetEnabledColor(kLabelColor);

  SetFocusBehavior(View::FocusBehavior::ALWAYS);
  SetToDefaultState();
}

void SharesheetExpandButton::SetToDefaultState() {
  icon_->SetImage(
      gfx::CreateVectorIcon(kCaretDownIcon, kCaretIconSize, kLabelColor));
  auto display_name = l10n_util::GetStringUTF16(IDS_SHARESHEET_MORE_APPS_LABEL);
  label_->SetText(display_name);
  SetAccessibleName(display_name);
}

void SharesheetExpandButton::SetToExpandedState() {
  icon_->SetImage(
      gfx::CreateVectorIcon(kCaretUpIcon, kCaretIconSize, kLabelColor));
  auto display_name =
      l10n_util::GetStringUTF16(IDS_SHARESHEET_FEWER_APPS_LABEL);
  label_->SetText(display_name);
  SetAccessibleName(display_name);
}

gfx::Size SharesheetExpandButton::CalculatePreferredSize() const {
  // Width is bubble width - left and right margins
  return gfx::Size((kDefaultBubbleWidth - 2 * kMarginSpacing), kHeight);
}

BEGIN_METADATA(SharesheetExpandButton, views::Button)
END_METADATA
