// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"

#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_throbber.h"

PageActionIconLoadingIndicatorView::PageActionIconLoadingIndicatorView(
    PageActionIconView* parent)
    : parent_(parent) {
  parent_->AddObserver(this);
  // Don't let the loading indicator process events.
  SetCanProcessEventsWithinSubtree(false);
}

PageActionIconLoadingIndicatorView::~PageActionIconLoadingIndicatorView() {
  parent_->RemoveObserver(this);
}

void PageActionIconLoadingIndicatorView::SetAnimating(bool animating) {
  if (!throbber_start_time_ == !animating)
    return;

  SetVisible(animating);
  if (animating) {
    throbber_start_time_ = base::TimeTicks::Now();
    animation_.StartThrobbing(-1);
  } else {
    throbber_start_time_.reset();
    animation_.Reset();
  }
  OnPropertyChanged(&throbber_start_time_, views::kPropertyEffectsNone);
}

bool PageActionIconLoadingIndicatorView::GetAnimating() const {
  return animation_.is_animating();
}

void PageActionIconLoadingIndicatorView::OnPaint(gfx::Canvas* canvas) {
  if (!throbber_start_time_)
    return;

  const SkColor color = GetColorProvider()->GetColor(ui::kColorThrobber);
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

BEGIN_METADATA(PageActionIconLoadingIndicatorView)
ADD_PROPERTY_METADATA(bool, Animating)
END_METADATA
