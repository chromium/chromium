// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/pointer_mark.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ttc {

namespace {

// Big enough to see at a glance, small enough to leave what is pointed at
// visible inside it.
constexpr int kDiameter = 40;
constexpr int kStrokeWidth = 3;

// The ring is there for long enough to be noticed, then fades.
constexpr base::TimeDelta kHoldDuration = base::Milliseconds(400);
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(400);

}  // namespace

std::unique_ptr<views::Widget> ShowPointerMark(gfx::NativeView parent,
                                               const gfx::Point& center) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.name = "TtcPointerMark";
  params.parent = parent;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.bounds =
      gfx::Rect(center - gfx::Vector2d(kDiameter / 2, kDiameter / 2),
                gfx::Size(kDiameter, kDiameter));
  auto widget = std::make_unique<views::Widget>(std::move(params));

  auto ring = std::make_unique<views::View>();
  ring->SetCanProcessEventsWithinSubtree(false);
  ring->SetBorder(views::CreateRoundedRectBorder(kStrokeWidth, kDiameter / 2.0f,
                                                 ui::kColorSysPrimary));
  // The ring fades on a layer of its own, not the window's. Some window
  // managers animate the window's layer when the window is shown, and a fade
  // started on that layer meanwhile would end at once, hiding the ring before
  // it was seen.
  ring->SetPaintToLayer();
  ring->layer()->SetFillsBoundsOpaquely(false);
  views::View* const ring_view = widget->SetContentsView(std::move(ring));
  // The ring has its own animation.
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->ShowInactive();

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&views::Widget::Hide, widget->GetWeakPtr()))
      .Once()
      .At(kHoldDuration)
      .SetDuration(kFadeDuration)
      .SetOpacity(ring_view->layer(), 0.0f);
  return widget;
}

}  // namespace ttc
