// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/overlay_scrollbar_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Total thickness of the thumb (matches visuals when hovered).
constexpr int kThumbThickness = 10;
constexpr int kThumbUnhoveredOffset = 5;
constexpr int kThumbUnhoveredTrailingPadding = 2;

}  // namespace

VerticalTabStripScrollBar::Thumb::Thumb(VerticalTabStripScrollBar* scroll_bar)
    : views::BaseScrollBarThumb(scroll_bar) {
  // |scroll_bar| isn't done being constructed; it's not safe to do anything
  // that might reference it yet.
}

VerticalTabStripScrollBar::Thumb::~Thumb() = default;

void VerticalTabStripScrollBar::Thumb::Init() {
  SetFlipCanvasOnPaintForRTLUI(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Animate all changes to the layer except the first one.
  OnStateChanged();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size VerticalTabStripScrollBar::Thumb::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  // The visual size of the thumb is kThumbThickness, but it slides back and
  // forth by thumb_hover_offset(). To make event targeting work well, expand
  // the width of the thumb such that it's always taking up the full width of
  // the track regardless of the offset.
  return gfx::Size(kThumbThickness + kThumbUnhoveredOffset,
                   kThumbThickness + kThumbUnhoveredOffset);
}

void VerticalTabStripScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(GetColorProvider()->GetColor(ui::kColorSysStateDisabled));
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF::TLBR(
      0, kThumbUnhoveredOffset, 0,
      GetState() == views::Button::STATE_NORMAL
          ? kThumbUnhoveredOffset + kThumbUnhoveredTrailingPadding
          : 0));
  float rounded_corners = fill_bounds.width() / 2.0f;
  canvas->DrawRoundRect(fill_bounds, rounded_corners, fill_flags);
}

void VerticalTabStripScrollBar::Thumb::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  Show();
}

void VerticalTabStripScrollBar::Thumb::OnStateChanged() {
  if (GetState() == views::Button::STATE_NORMAL) {
    gfx::Transform translation;
    const int direction = base::i18n::IsRTL() ? -1 : 1;
    translation.Translate(gfx::Vector2d(direction * kThumbUnhoveredOffset, 0));
    layer()->SetTransform(translation);

    if (GetWidget()) {
      StartHideCountdown();
    }
  } else {
    hide_timer_.Stop();
    Show();
    layer()->SetTransform(gfx::Transform());
  }
  SchedulePaint();
}

void VerticalTabStripScrollBar::Thumb::Show() {
  if (layer()->GetTargetOpacity() != 1.0f) {
    layer()->SetOpacity(1.0f);
  }
  hide_timer_.Stop();
  // Don't start the hide countdown if the thumb is still hovered or pressed.
  if (GetState() == views::Button::STATE_NORMAL) {
    StartHideCountdown();
  }
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

VerticalTabStripScrollBar::VerticalTabStripScrollBar()
    : views::ScrollBar(views::ScrollBar::Orientation::kVertical) {
  // Allow the thumb to take up the whole size of the scrollbar.  Layout need
  // only set the thumb cross-axis coordinate; ScrollBar::Update() will set the
  // thumb size/offset.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* thumb = new Thumb(this);
  SetThumb(thumb);
  thumb->Init();
}

VerticalTabStripScrollBar::~VerticalTabStripScrollBar() = default;

gfx::Insets VerticalTabStripScrollBar::GetInsets() const {
  return gfx::Insets::TLBR(0, -kThumbUnhoveredOffset, 0, 0);
}

bool VerticalTabStripScrollBar::OverlapsContent() const {
  return true;
}

gfx::Rect VerticalTabStripScrollBar::GetTrackBounds() const {
  return GetContentsBounds();
}

int VerticalTabStripScrollBar::GetThickness() const {
  return kThumbThickness;
}

BEGIN_METADATA(VerticalTabStripScrollBar)
END_METADATA
