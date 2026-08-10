// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"

#include <algorithm>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kTabScrollButtonContainer);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kStartScrollButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kEndScrollButton);

namespace {
// TODO(b/523328731): Validate animation parameters with design.
constexpr base::TimeDelta kScrollAnimationTime = base::Milliseconds(300);
constexpr int kScrollButtonSpacing = 1;
}  // namespace

TabScrollButtonContainer::TabScrollButtonContainer(
    BrowserWindowInterface* browser_window_interface) {
  SetProperty(views::kElementIdentifierKey, kTabScrollButtonContainer);

  std::unique_ptr<views::BoxLayout> box_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing_=*/kScrollButtonSpacing,
          /*collapse_margins_spacing=*/false);
  box_layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));
  SetMirrored(false);

  start_scroll_button_ = AddChildView(std::make_unique<TabStripControlButton>(
      browser_window_interface,
      views::Button::PressedCallback(base::BindRepeating(
          &TabScrollButtonContainer::BeginScrollAnimation,
          base::Unretained(this), /*scroll_to_start=*/true)),
      kKeyboardArrowLeftIcon, Edge::kNone, Edge::kNone));
  start_scroll_button_->SetProperty(views::kElementIdentifierKey,
                                    kStartScrollButton);
  start_scroll_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(base::i18n::IsRTL()
                                    ? IDS_ACCNAME_TAB_SCROLL_TRAILING
                                    : IDS_ACCNAME_TAB_SCROLL_LEADING));

  end_scroll_button_ = AddChildView(std::make_unique<TabStripControlButton>(
      browser_window_interface,
      views::Button::PressedCallback(base::BindRepeating(
          &TabScrollButtonContainer::BeginScrollAnimation,
          base::Unretained(this), /*scroll_to_start=*/false)),
      kKeyboardArrowRightIcon, Edge::kNone, Edge::kNone));
  end_scroll_button_->SetProperty(views::kElementIdentifierKey,
                                  kEndScrollButton);
  end_scroll_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      base::i18n::IsRTL() ? IDS_ACCNAME_TAB_SCROLL_LEADING
                          : IDS_ACCNAME_TAB_SCROLL_TRAILING));
  start_scroll_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  end_scroll_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  animation_.SetDuration(kScrollAnimationTime);
}

bool TabScrollButtonContainer::IsPositionInWindowCaption(const gfx::Point& p) {
  return !start_scroll_button_->HitTestPoint(
             ConvertPointToTarget(this, start_scroll_button_, p)) &&
         !end_scroll_button_->HitTestPoint(
             ConvertPointToTarget(this, end_scroll_button_, p));
}

void TabScrollButtonContainer::SetScrollView(views::ScrollView* scroll_view) {
  scroll_view_ = scroll_view;

  if (scroll_view == nullptr) {
    animation_.Stop();
  }
}

void TabScrollButtonContainer::BeginScrollAnimation(bool scroll_to_start) {
  CHECK(scroll_view_);
  views::ScrollBar* scroll_bar = scroll_view_->horizontal_scroll_bar();

  if (animation_.is_animating()) {
    animation_.Stop();
  }

  int full_scroll_amount =
      scroll_view_->GetScrollIncrement(scroll_bar, /*is_page=*/true,
                                       /*is_positive=*/true);
  int current_offset = static_cast<int>(scroll_view_->CurrentOffset().x());

  // We may not scroll the entire `full_scroll_amount`, if the current offset
  // is less than `full_scroll_amount` from the start and we are scrolling left,
  // or the current offset is less than `full_scroll_amount` away from the
  // end and we are scrolling right.
  int actual_scroll_amount = full_scroll_amount;

  if (scroll_to_start) {
    actual_scroll_amount = std::max(
        0, base::i18n::IsRTL() ? scroll_bar->GetMaxPosition() - current_offset
                               : current_offset - scroll_bar->GetMinPosition());
  } else {
    actual_scroll_amount = std::max(
        0, base::i18n::IsRTL() ? current_offset - scroll_bar->GetMinPosition()
                               : scroll_bar->GetMaxPosition() - current_offset);
  }
  actual_scroll_amount = std::min(full_scroll_amount, actual_scroll_amount);

  if (actual_scroll_amount <= 0) {
    animation_params_ = std::nullopt;
    return;
  }

  animation_params_ = AnimationParams{
      .scroll_to_start = scroll_to_start,
      .amount_to_scroll = actual_scroll_amount,
      .last_progress = 0,
  };

  animation_.Start();
}

void TabScrollButtonContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK(scroll_view_);
  CHECK(animation_params_.has_value());

  float progress = gfx::Tween::CalculateValue(gfx::Tween::Type::EASE_OUT,
                                              animation_.GetCurrentValue());
  float progress_since_last_scroll =
      std::max(0.0f, progress - animation_params_->last_progress);
  float need_to_scroll =
      animation_params_->amount_to_scroll * progress_since_last_scroll;
  int sign = animation_params_->scroll_to_start ? -1 : 1;
  sign *= base::i18n::IsRTL() ? -1 : 1;

  scroll_view_->ScrollByOffset({sign * need_to_scroll, 0});
  animation_params_->last_progress = progress;
}

void TabScrollButtonContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_params_ = std::nullopt;
}

void TabScrollButtonContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  animation_params_ = std::nullopt;
}

BEGIN_METADATA(TabScrollButtonContainer)
END_METADATA
