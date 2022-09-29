// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/new_badge_label.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace user_education {

NewBadgeLabel::NewBadgeLabel(const std::u16string& text,
                             int text_context,
                             int text_style,
                             gfx::DirectionalityMode directionality_mode)
    : Label(text, text_context, text_style, directionality_mode) {
  UpdatePaddingForNewBadge();
}

NewBadgeLabel::NewBadgeLabel(const std::u16string& text, const CustomFont& font)
    : Label(text, font) {
  UpdatePaddingForNewBadge();
}

NewBadgeLabel::~NewBadgeLabel() = default;

void NewBadgeLabel::SetDisplayNewBadge(bool display_new_badge) {
  DCHECK(!GetWidget() || !GetVisible() || !GetWidget()->IsVisible())
      << "New badge display should not be toggled while this element is "
         "visible.";
  if (display_new_badge_ == display_new_badge)
    return;

  display_new_badge_ = display_new_badge;

  // At this point we know the display setting has changed, so we must add or
  // remove the relevant padding and insets.
  if (display_new_badge_) {
    UpdatePaddingForNewBadge();
  } else {
    // Clearing these only when display is set to false - rather than in e.g.
    // UpdatePaddingForNewBadge() - ensures that any subsequent modifications to
    // the border or padding are not discarded.
    views::Label::SetBorder(nullptr);
    ClearProperty(views::kInternalPaddingKey);
  }

  OnPropertyChanged(&display_new_badge_, views::kPropertyEffectsLayout);
}

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
  std::u16string accessible_name = GetText();
  if (display_new_badge_) {
    accessible_name.push_back(' ');
    accessible_name.append(views::NewBadge::GetNewBadgeAccessibleDescription());
  }
  node_data->SetNameChecked(accessible_name);
}

gfx::Size NewBadgeLabel::CalculatePreferredSize() const {
  gfx::Size size = Label::CalculatePreferredSize();
  if (display_new_badge_)
    size.SetToMax(views::NewBadge::GetNewBadgeSize(font_list()));
  return size;
}

gfx::Size NewBadgeLabel::GetMinimumSize() const {
  gfx::Size size = Label::GetMinimumSize();
  if (display_new_badge_)
    size.SetToMax(views::NewBadge::GetNewBadgeSize(font_list()));
  return size;
}

int NewBadgeLabel::GetHeightForWidth(int w) const {
  int height = Label::GetHeightForWidth(w);
  if (display_new_badge_) {
    height = std::max(height,
                      views::NewBadge::GetNewBadgeSize(font_list()).height());
  }
  return height;
}

void NewBadgeLabel::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                               float new_device_scale_factor) {
  UpdatePaddingForNewBadge();
}

void NewBadgeLabel::OnPaint(gfx::Canvas* canvas) {
  Label::OnPaint(canvas);
  if (!display_new_badge_)
    return;
  const gfx::Rect contents_bounds = GetContentsBounds();
  int extra_width = 0;
  if (badge_placement_ == BadgePlacement::kImmediatelyAfterText)
    extra_width = std::max(0, width() - GetPreferredSize().width());
  const int badge_x = views::NewBadge::kNewBadgeHorizontalMargin - extra_width +
                      (base::i18n::IsRTL() ? width() - contents_bounds.x()
                                           : contents_bounds.right());

  views::NewBadge::DrawNewBadge(canvas, this, badge_x, GetFontListY(),
                                font_list());
}

void NewBadgeLabel::UpdatePaddingForNewBadge() {
  if (!display_new_badge_)
    return;

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
  views::Label::SetBorder(views::CreateEmptyBorder(border));

  // If there is right-padding, ensure that layouts understand it can be
  // collapsed into a margin.
  SetProperty(views::kInternalPaddingKey,
              gfx::Insets::TLBR(0, 0, 0, right_padding));
}

void NewBadgeLabel::SetBorder(std::unique_ptr<views::Border> b) {
  NOTREACHED() << "Calling SetBorder() externally is currently not allowed.";
}

BEGIN_METADATA(NewBadgeLabel, views::Label)
ADD_PROPERTY_METADATA(bool, DisplayNewBadge)
ADD_PROPERTY_METADATA(NewBadgeLabel::BadgePlacement, BadgePlacement)
ADD_PROPERTY_METADATA(bool, PadAfterNewBadge)
END_METADATA

}  // namespace user_education

DEFINE_ENUM_CONVERTERS(
    user_education::NewBadgeLabel::BadgePlacement,
    {user_education::NewBadgeLabel::BadgePlacement::kImmediatelyAfterText,
     u"kImmediatelyAfterText"},
    {user_education::NewBadgeLabel::BadgePlacement::kTrailingEdge,
     u"kTrailingEdge"})
