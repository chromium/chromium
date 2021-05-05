// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"

#include "ash/public/cpp/ash_typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Sizes are in px.
constexpr int kDefaultBubbleWidth = 416;
constexpr int kCaretIconSize = 20;
constexpr int kHeight = 32;
constexpr int kBetweenChildSpacing = 8;
constexpr int kMarginSpacing = 24;

}  // namespace

namespace ash {
namespace sharesheet {

SharesheetExpandButton::SharesheetExpandButton(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(6, 16, 6, 16),
      kBetweenChildSpacing, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<views::ImageView>());

  label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), CONTEXT_SHARESHEET_BUBBLE_BODY, STYLE_SHARESHEET));
  label_->SetLineHeight(kPrimaryTextLineHeight);
  label_->SetEnabledColor(kButtonTextColor);

  SetFocusBehavior(View::FocusBehavior::ALWAYS);
  SetToDefaultState();
}

void SharesheetExpandButton::SetToDefaultState() {
  icon_->SetImage(gfx::CreateVectorIcon(vector_icons::kCaretDownIcon,
                                        kCaretIconSize, kButtonTextColor));
  auto display_name = l10n_util::GetStringUTF16(IDS_SHARESHEET_MORE_APPS_LABEL);
  label_->SetText(display_name);
  SetAccessibleName(display_name);
}

void SharesheetExpandButton::SetToExpandedState() {
  icon_->SetImage(gfx::CreateVectorIcon(vector_icons::kCaretUpIcon,
                                        kCaretIconSize, kButtonTextColor));
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

}  // namespace sharesheet
}  // namespace ash
