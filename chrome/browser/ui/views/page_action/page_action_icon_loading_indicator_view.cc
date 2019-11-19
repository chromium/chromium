// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"

#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_throbber.h"

PageActionIconLoadingIndicatorView::PageActionIconLoadingIndicatorView(
    PageActionIconView* parent)
    : parent_(parent) {
  parent_->AddObserver(this);
  // Don't let the loading indicator process events.
  set_can_process_events_within_subtree(false);
}

PageActionIconLoadingIndicatorView::~PageActionIconLoadingIndicatorView() {
  parent_->RemoveObserver(this);
}

void PageActionIconLoadingIndicatorView::ShowAnimation() {
  if (!throbber_start_time_)
    throbber_start_time_ = base::TimeTicks::Now();

  SetVisible(true);
  animation_.StartThrobbing(-1);
}

void PageActionIconLoadingIndicatorView::StopAnimation() {
  throbber_start_time_.reset();
  SetVisible(false);
  animation_.Reset();
}

bool PageActionIconLoadingIndicatorView::IsAnimating() {
  return animation_.is_animating();
}

void PageActionIconLoadingIndicatorView::OnPaint(gfx::Canvas* canvas) {
  if (!throbber_start_time_)
    return;

  const SkColor color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TAB_THROBBER_SPINNING);
  constexpr int kThrobberStrokeWidth = 2;
  gfx::PaintThrobberSpinning(canvas, GetLocalBounds(), color,
                             base::TimeTicks::Now() - *throbber_start_time_,
                             kThrobberStrokeWidth);
}

void PageActionIconLoadingIndicatorView::OnViewBoundsChanged(
    views::View* observed_view) {
  SetBoundsRect(observed_view->GetLocalBounds());
}

void PageActionIconLoadingIndicatorView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &animation_);
  SchedulePaint();
}
