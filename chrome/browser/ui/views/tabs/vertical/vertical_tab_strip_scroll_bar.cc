// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
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

namespace {

// Total thickness of the thumb.
constexpr int kThumbThickness = 5;
constexpr int kThumbTrailingPadding = 4;
constexpr int kCollapsedThumbTrailingPadding = 0;

}  // namespace

VerticalTabStripScrollBar::Thumb::Thumb(VerticalTabStripScrollBar* scroll_bar)
    : views::BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {
  // |scroll_bar| isn't done being constructed; it's not safe to do anything
  // that might reference it yet.
}

VerticalTabStripScrollBar::Thumb::~Thumb() = default;

void VerticalTabStripScrollBar::Thumb::Init() {
  SetFlipCanvasOnPaintForRTLUI(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  StartHideCountdown();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size VerticalTabStripScrollBar::Thumb::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  const int padding = scroll_bar_->tab_strip_collapsed_
                          ? kCollapsedThumbTrailingPadding
                          : kThumbTrailingPadding;
  return gfx::Size(kThumbThickness + padding, kThumbThickness + padding);
}

void VerticalTabStripScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(GetColorProvider()->GetColor(ui::kColorSysStateDisabled));
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF::TLBR(
      0, 0, 0,
      (scroll_bar_->tab_strip_collapsed_ ? kCollapsedThumbTrailingPadding
                                         : kThumbTrailingPadding)));
  float rounded_corners = fill_bounds.width() / 2.0f;
  canvas->DrawRoundRect(fill_bounds, rounded_corners, fill_flags);
}

void VerticalTabStripScrollBar::Thumb::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  Show();
  // Don't start the hide countdown if the thumb is still hovered or pressed.
  if (GetState() == views::Button::STATE_NORMAL) {
    StartHideCountdown();
  }
}

void VerticalTabStripScrollBar::Thumb::Show() {
  if (layer()->GetTargetOpacity() != 1.0f) {
    layer()->SetOpacity(1.0f);
  }
  hide_timer_.Stop();
}

void VerticalTabStripScrollBar::Thumb::Hide() {
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(ui::GetOverlayScrollbarFadeDuration());
  if (layer()->GetTargetOpacity() != 0.0f) {
    layer()->SetOpacity(0.0f);
  }
}

void VerticalTabStripScrollBar::Thumb::StartHideCountdown() {
  hide_timer_.Start(FROM_HERE, ui::GetOverlayScrollbarFadeDelay(),
                    base::BindOnce(&VerticalTabStripScrollBar::Thumb::Hide,
                                   base::Unretained(this)));
}

BEGIN_METADATA(VerticalTabStripScrollBar, Thumb)
END_METADATA

VerticalTabStripScrollBar::VerticalTabStripScrollBar(
    tabs::VerticalTabStripStateController* state_controller)
    : views::ScrollBar(views::ScrollBar::Orientation::kVertical),
      tab_strip_collapsed_(state_controller->IsCollapsed()) {
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* thumb = new Thumb(this);
  SetThumb(thumb);
  thumb->Init();
  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripScrollBar::OnCollapsedStateChanged,
          base::Unretained(this)));
}

VerticalTabStripScrollBar::~VerticalTabStripScrollBar() = default;

void VerticalTabStripScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  VerticalTabStripScrollBar::Thumb* thumb =
      static_cast<VerticalTabStripScrollBar::Thumb*>(GetThumb());
  thumb->Show();
}

void VerticalTabStripScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  VerticalTabStripScrollBar::Thumb* thumb =
      static_cast<VerticalTabStripScrollBar::Thumb*>(GetThumb());
  thumb->StartHideCountdown();
}

bool VerticalTabStripScrollBar::OverlapsContent() const {
  return true;
}

gfx::Rect VerticalTabStripScrollBar::GetTrackBounds() const {
  return GetContentsBounds();
}

int VerticalTabStripScrollBar::GetThickness() const {
  return kThumbThickness + (tab_strip_collapsed_
                                ? kCollapsedThumbTrailingPadding
                                : kThumbTrailingPadding);
}

void VerticalTabStripScrollBar::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* state_controller) {
  if (tab_strip_collapsed_ != state_controller->IsCollapsed()) {
    tab_strip_collapsed_ = state_controller->IsCollapsed();
    InvalidateLayout();
  }
}

BEGIN_METADATA(VerticalTabStripScrollBar)
END_METADATA
