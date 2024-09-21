// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/new_badge_label.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/badge_painter.h"
#include "ui/views/border.h"
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
  UpdateAccessibleName();
}

NewBadgeLabel::NewBadgeLabel(const std::u16string& text, const CustomFont& font)
    : Label(text, font) {
  UpdatePaddingForNewBadge();
  UpdateAccessibleName();
}

NewBadgeLabel::~NewBadgeLabel() = default;

void NewBadgeLabel::SetDisplayNewBadge(DisplayNewBadge display_new_badge) {
  SetDisplayNewBadgeImpl(display_new_badge);
}

void NewBadgeLabel::SetDisplayNewBadgeForTesting(bool display_new_badge) {
  SetDisplayNewBadgeImpl(display_new_badge);
}

void NewBadgeLabel::SetDisplayNewBadgeImpl(bool display_new_badge) {
  DCHECK(!GetWidget() || !GetVisible() || !GetWidget()->IsVisible())
      << "New badge display should not be toggled while this element is "
         "visible.";
  if (display_new_badge_ == display_new_badge)
    return;

  display_new_badge_ = display_new_badge;
  UpdateAccessibleName();

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

gfx::Size NewBadgeLabel::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = Label::CalculatePreferredSize(available_size);
  if (display_new_badge_)
    size.SetToMax(GetNewBadgeSize());
  return size;
}

gfx::Size NewBadgeLabel::GetMinimumSize() const {
  gfx::Size size = Label::GetMinimumSize();
  if (display_new_badge_)
    size.SetToMax(GetNewBadgeSize());
  return size;
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
    extra_width = std::max(
        0, width() - GetPreferredSize(views::SizeBounds(width(), {})).width());
  const int badge_x = views::BadgePainter::kBadgeHorizontalMargin -
                      extra_width +
                      (base::i18n::IsRTL() ? width() - contents_bounds.x()
                                           : contents_bounds.right());

  views::BadgePainter::PaintBadge(canvas, this, badge_x, GetFontListY(),
                                  new_badge_text_, font_list());
}

void NewBadgeLabel::SetText(const std::u16string& text) {
  views::Label::SetText(text);
  UpdateAccessibleName();
}

void NewBadgeLabel::UpdatePaddingForNewBadge() {
  if (!display_new_badge_)
    return;

  // Calculate the width required for the badge plus separation from the text.
  int width = GetNewBadgeSize().width();
  int right_padding = 0;
  if (pad_after_new_badge_) {
    width += 2 * views::BadgePainter::kBadgeHorizontalMargin;
    right_padding = views::BadgePainter::kBadgeHorizontalMargin;
  } else {
    width += views::BadgePainter::kBadgeHorizontalMargin;
  }

  // Reserve adequate space above and below the label so that the badge will fit
  // vertically, and to the right to actually hold the badge.
  gfx::Insets border = gfx::AdjustVisualBorderForFont(
      font_list(), gfx::Insets(views::BadgePainter::kBadgeInternalPadding));
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

gfx::Size NewBadgeLabel::GetNewBadgeSize() const {
  return views::BadgePainter::GetBadgeSize(new_badge_text_, font_list());
}

std::u16string NewBadgeLabel::GetAccessibleDescription() const {
  return l10n_util::GetStringUTF16(IDS_NEW_BADGE_SCREEN_READER_MESSAGE);
}

void NewBadgeLabel::SetBorder(std::unique_ptr<views::Border> b) {
  NOTREACHED_IN_MIGRATION()
      << "Calling SetBorder() externally is currently not allowed.";
}

void NewBadgeLabel::UpdateAccessibleName() {
  std::u16string accessible_name = GetText();
  if (display_new_badge_) {
    accessible_name.push_back(' ');
    accessible_name.append(GetAccessibleDescription());
    GetViewAccessibility().SetName(accessible_name);
  }
}

BEGIN_METADATA(NewBadgeLabel)
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
