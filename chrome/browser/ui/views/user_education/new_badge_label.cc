// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/new_badge_label.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

NewBadgeLabel::NewBadgeLabel(const base::string16& text,
                             int text_context,
                             int text_style,
                             gfx::DirectionalityMode directionality_mode)
    : Label(text, text_context, text_style, directionality_mode) {
  UpdatePaddingForNewBadge();
}

NewBadgeLabel::NewBadgeLabel(const base::string16& text, const CustomFont& font)
    : Label(text, font) {
  UpdatePaddingForNewBadge();
}

NewBadgeLabel::~NewBadgeLabel() = default;

void NewBadgeLabel::SetPadAfterNewBadge(bool pad_after_new_badge) {
  if (pad_after_new_badge_ == pad_after_new_badge)
    return;

  pad_after_new_badge_ = pad_after_new_badge;
  UpdatePaddingForNewBadge();
  OnPropertyChanged(&pad_after_new_badge_, views::kPropertyEffectsLayout);
}

void NewBadgeLabel::SetBadgePlacement(BadgePlacement badge_placement) {
  if (badge_placement_ == badge_placement)
    return;

  badge_placement_ = badge_placement;
  UpdatePaddingForNewBadge();
  OnPropertyChanged(&badge_placement_, views::kPropertyEffectsPaint);
}

void NewBadgeLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Label::GetAccessibleNodeData(node_data);
  base::string16 accessible_name = GetText();
  accessible_name.push_back(' ');
  accessible_name.append(views::NewBadge::GetNewBadgeAccessibleDescription());
  node_data->SetName(accessible_name);
}

gfx::Size NewBadgeLabel::CalculatePreferredSize() const {
  gfx::Size size = Label::CalculatePreferredSize();
  size.SetToMax(views::NewBadge::GetNewBadgeSize(font_list()));
  return size;
}

gfx::Size NewBadgeLabel::GetMinimumSize() const {
  gfx::Size size = Label::GetMinimumSize();
  size.SetToMax(views::NewBadge::GetNewBadgeSize(font_list()));
  return size;
}

int NewBadgeLabel::GetHeightForWidth(int w) const {
  return std::max(Label::GetHeightForWidth(w),
                  views::NewBadge::GetNewBadgeSize(font_list()).height());
}

void NewBadgeLabel::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                               float new_device_scale_factor) {
  UpdatePaddingForNewBadge();
}

void NewBadgeLabel::OnPaint(gfx::Canvas* canvas) {
  Label::OnPaint(canvas);
  const gfx::Rect contents_bounds = GetContentsBounds();
  int extra_width = 0;
  if (badge_placement_ == BadgePlacement::kImmediatelyAfterText)
    extra_width = std::max(0, width() - GetPreferredSize().width());
  const int badge_x = views::NewBadge::kNewBadgeHorizontalMargin - extra_width +
                      (base::i18n::IsRTL() ? width() - contents_bounds.x()
                                           : contents_bounds.right());
  int top = contents_bounds.y();
  switch (GetVerticalAlignment()) {
    case gfx::VerticalAlignment::ALIGN_TOP:
      break;
    case gfx::VerticalAlignment::ALIGN_MIDDLE:
      top += (contents_bounds.height() - font_list().GetHeight()) / 2;
      break;
    case gfx::VerticalAlignment::ALIGN_BOTTOM:
      top += contents_bounds.height() - font_list().GetHeight();
      break;
  }

  views::NewBadge::DrawNewBadge(canvas, this, badge_x, top, font_list());
}

void NewBadgeLabel::UpdatePaddingForNewBadge() {
  // Calculate the width required for the badge plus separation from the text.
  int width = views::NewBadge::GetNewBadgeSize(font_list()).width();
  int right_padding = 0;
  if (pad_after_new_badge_) {
    width += 2 * views::NewBadge::kNewBadgeHorizontalMargin;
    right_padding = views::NewBadge::kNewBadgeHorizontalMargin;
  } else {
    width += views::NewBadge::kNewBadgeHorizontalMargin;
  }

  // Reserve adequate space above and below the label so that the badge will fit
  // vertically, and to the right to actually hold the badge.
  gfx::Insets border = gfx::AdjustVisualBorderForFont(
      font_list(), gfx::Insets(views::NewBadge::kNewBadgeInternalPadding));
  if (base::i18n::IsRTL()) {
    border.set_left(width);
    border.set_right(0);
  } else {
    border.set_left(0);
    border.set_right(width);
  }
  SetBorder(views::CreateEmptyBorder(border));

  // If there is right-padding, ensure that layouts understand it can be
  // collapsed into a margin.
  SetProperty(views::kInternalPaddingKey, gfx::Insets(0, 0, 0, right_padding));
}

DEFINE_ENUM_CONVERTERS(NewBadgeLabel::BadgePlacement,
                       {NewBadgeLabel::BadgePlacement::kImmediatelyAfterText,
                        base::ASCIIToUTF16("kImmediatelyAfterText")},
                       {NewBadgeLabel::BadgePlacement::kTrailingEdge,
                        base::ASCIIToUTF16("kTrailingEdge")})

BEGIN_METADATA(NewBadgeLabel, views::Label)
ADD_PROPERTY_METADATA(NewBadgeLabel::BadgePlacement, BadgePlacement)
ADD_PROPERTY_METADATA(bool, PadAfterNewBadge)
END_METADATA
