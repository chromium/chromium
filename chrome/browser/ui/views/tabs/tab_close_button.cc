// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_close_button.h"

#include <map>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/rect_based_targeting_utils.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace {
constexpr int kGlyphWidth = 16;
constexpr int kTouchGlyphWidth = 24;

class TabCloseButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  TabCloseButtonHighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    const gfx::Rect bounds = view->GetContentsBounds();
    const gfx::Point center = bounds.CenterPoint();
    const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
        views::EMPHASIS_MAXIMUM, bounds.size());
    return SkPath().addCircle(center.x(), center.y(), radius);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseButtonHighlightPathGenerator);
};

}  //  namespace

TabCloseButton::TabCloseButton(views::ButtonListener* listener,
                               MouseEventCallback mouse_event_callback)
    : views::ImageButton(listener),
      mouse_event_callback_(std::move(mouse_event_callback)) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_highlight_opacity(0.16f);
  set_ink_drop_visible_opacity(0.14f);

  // Disable animation so that the hover indicator shows up immediately to help
  // avoid mis-clicks.
  SetAnimationDuration(base::TimeDelta());
  GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());

  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<TabCloseButtonHighlightPathGenerator>());
}

TabCloseButton::~TabCloseButton() {}

// static
int TabCloseButton::GetWidth() {
  return ui::MaterialDesignController::touch_ui() ? kTouchGlyphWidth
                                                  : kGlyphWidth;
}

void TabCloseButton::SetIconColors(SkColor foreground_color,
                                   SkColor background_color) {
  icon_color_ = foreground_color;
  set_ink_drop_base_color(
      color_utils::GetColorWithMaxContrast(background_color));
}

const char* TabCloseButton::GetClassName() const {
  return "TabCloseButton";
}

views::View* TabCloseButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tab close button has no children, so tooltip handler should be the same
  // as the event handler. In addition, a hit test has to be performed for the
  // point (as GetTooltipHandlerForPoint() is responsible for it).
  if (!HitTestPoint(point))
    return nullptr;
  return GetEventHandlerForPoint(point);
}

bool TabCloseButton::OnMousePressed(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);

  bool handled = ImageButton::OnMousePressed(event);
  // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
  // sees them.
  return !event.IsMiddleMouseButton() && handled;
}

void TabCloseButton::OnMouseReleased(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseReleased(event);
}

void TabCloseButton::OnMouseMoved(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseMoved(event);
}

void TabCloseButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

gfx::Size TabCloseButton::CalculatePreferredSize() const {
  int width = GetWidth();
  gfx::Size size(width, width);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

std::unique_ptr<views::InkDropMask> TabCloseButton::CreateInkDropMask() const {
  const gfx::Rect bounds = GetContentsBounds();
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, bounds.size());
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetMirroredRect(bounds).CenterPoint(), radius);
}

void TabCloseButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  constexpr float kStrokeWidth = 1.5f;
  float touch_scale = float{GetWidth()} / kGlyphWidth;
  float size = (kGlyphWidth - 8) * touch_scale - kStrokeWidth;
  gfx::RectF glyph_bounds(GetContentsBounds());
  glyph_bounds.ClampToCenteredSize(gfx::SizeF(size, size));
  flags.setAntiAlias(true);
  flags.setStrokeWidth(kStrokeWidth);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setColor(icon_color_);
  canvas->DrawLine(glyph_bounds.origin(), glyph_bounds.bottom_right(), flags);
  canvas->DrawLine(glyph_bounds.bottom_left(), glyph_bounds.top_right(), flags);
}

views::View* TabCloseButton::TargetForRect(views::View* root,
                                           const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return ViewTargeterDelegate::TargetForRect(root, rect);

  // Ignore the padding set on the button.
  gfx::Rect contents_bounds = GetMirroredRect(GetContentsBounds());

#if defined(USE_AURA)
  // Include the padding in hit-test for touch events.
  // TODO(pkasting): It seems like touch events would generate rects rather
  // than points and thus use the TargetForRect() call above.  If this is
  // reached, it may be from someone calling GetEventHandlerForPoint() while a
  // touch happens to be occurring.  In such a case, maybe we don't want this
  // code to run?  It's possible this block should be removed, or maybe this
  // whole function deleted.  Note that in these cases, we should probably
  // also remove the padding on the close button bounds (see Tab::Layout()),
  // as it will be pointless.
  if (aura::Env::GetInstance()->is_touch_down())
    contents_bounds = GetLocalBounds();
#endif

  return contents_bounds.Intersects(rect) ? this : parent();
}

bool TabCloseButton::GetHitTestMask(SkPath* mask) const {
  // We need to define this so hit-testing won't include the border region.
  mask->addRect(gfx::RectToSkRect(GetMirroredRect(GetContentsBounds())));
  return true;
}
