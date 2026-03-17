// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/rounded_scroll_bar.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/overlay_scrollbar_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"

namespace tabs {

namespace {

// Total thickness of the thumb.
constexpr int kThumbThickness = 5;
constexpr int kThumbTrailingPadding = 4;

}  // namespace

RoundedScrollBar::Thumb::Thumb(RoundedScrollBar* scroll_bar)
    : views::BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {
  // |scroll_bar| isn't done being constructed; it's not safe to do anything
  // that might reference it yet.
}

RoundedScrollBar::Thumb::~Thumb() = default;

void RoundedScrollBar::Thumb::Init() {
  SetFlipCanvasOnPaintForRTLUI(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  StartHideCountdown();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size RoundedScrollBar::Thumb::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  const int padding =
      scroll_bar_->ShouldHaveRightMargin() ? kThumbTrailingPadding : 0;
  return gfx::Size(kThumbThickness + padding, kThumbThickness + padding);
}

void RoundedScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(GetColorProvider()->GetColor(ui::kColorSysStateDisabled));
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF::TLBR(
      0, 0, 0,
      (scroll_bar_->ShouldHaveRightMargin() ? kThumbTrailingPadding : 0)));
  float rounded_corners = fill_bounds.width() / 2.0f;
  canvas->DrawRoundRect(fill_bounds, rounded_corners, fill_flags);
}

void RoundedScrollBar::Thumb::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // When the host view is animating its size (i.e. during collapse or expand),
  // the scrollbar thumb should be hidden and not update its bounds until the
  // animation is complete.
  if (is_animating_size_) {
    Hide();
    return;
  }
  Show();
  // Don't start the hide countdown if the thumb is still hovered or pressed.
  if (GetState() == views::Button::STATE_NORMAL) {
    StartHideCountdown();
  }
}

void RoundedScrollBar::Thumb::Show() {
  if (layer()->GetTargetOpacity() != 1.0f) {
    layer()->SetOpacity(1.0f);
  }
  hide_timer_.Stop();
}

void RoundedScrollBar::Thumb::Hide() {
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(ui::GetOverlayScrollbarFadeDuration());
  if (layer()->GetTargetOpacity() != 0.0f) {
    layer()->SetOpacity(0.0f);
  }
}

void RoundedScrollBar::Thumb::StartHideCountdown() {
  hide_timer_.Start(
      FROM_HERE, ui::GetOverlayScrollbarFadeDelay(),
      base::BindOnce(&RoundedScrollBar::Thumb::Hide, base::Unretained(this)));
}

BEGIN_METADATA(RoundedScrollBar, Thumb)
END_METADATA

RoundedScrollBar::RoundedScrollBar()
    : views::ScrollBar(views::ScrollBar::Orientation::kVertical) {
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* thumb = new Thumb(this);
  SetThumb(thumb);
  thumb->Init();
}

RoundedScrollBar::~RoundedScrollBar() = default;

void RoundedScrollBar::SetIsAnimatingSize(bool is_animating) {
  Thumb* thumb = views::AsViewClass<Thumb>(GetThumb());
  thumb->set_is_animating_size(is_animating);
  if (is_animating) {
    thumb->Hide();
  }
}

void RoundedScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  RoundedScrollBar::Thumb* thumb =
      views::AsViewClass<RoundedScrollBar::Thumb>(GetThumb());
  thumb->Show();
}

void RoundedScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  RoundedScrollBar::Thumb* thumb =
      views::AsViewClass<RoundedScrollBar::Thumb>(GetThumb());
  thumb->StartHideCountdown();
}

bool RoundedScrollBar::OverlapsContent() const {
  return true;
}

gfx::Rect RoundedScrollBar::GetTrackBounds() const {
  return GetContentsBounds();
}

int RoundedScrollBar::GetThickness() const {
  return kThumbThickness +
         (ShouldHaveRightMargin() ? kThumbTrailingPadding : 0);
}

bool RoundedScrollBar::ShouldHaveRightMargin() const {
  return true;
}

BEGIN_METADATA(RoundedScrollBar)
END_METADATA

}  // namespace tabs
