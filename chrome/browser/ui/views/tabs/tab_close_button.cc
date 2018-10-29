// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_close_button.h"

#include <map>
#include <memory>
#include <vector>

#include "base/hash.h"
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
#include "ui/views/rect_based_targeting_utils.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace {
constexpr int kGlyphWidth = 16;
constexpr int kTouchGlyphWidth = 24;
}  //  namespace

TabCloseButton::TabCloseButton(views::ButtonListener* listener,
                               MouseEventCallback mouse_event_callback)
    : views::ImageButton(listener),
      mouse_event_callback_(std::move(mouse_event_callback)) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  SetAnimationDuration(0);

  SetInstallFocusRingOnFocus(true);
  SetFocusPainter(nullptr);
}

TabCloseButton::~TabCloseButton() {}

// static
int TabCloseButton::GetWidth() {
  return ui::MaterialDesignController::touch_ui() ? kTouchGlyphWidth
                                                  : kGlyphWidth;
}

void TabCloseButton::SetIconColors(SkColor icon_color,
                                   SkColor hovered_icon_color,
                                   SkColor pressed_icon_color,
                                   SkColor hovered_color,
                                   SkColor pressed_color) {
  icon_colors_[views::Button::STATE_NORMAL] = icon_color;
  icon_colors_[views::Button::STATE_HOVERED] = hovered_icon_color;
  icon_colors_[views::Button::STATE_PRESSED] = pressed_icon_color;
  highlight_colors_[views::Button::STATE_HOVERED] = hovered_color;
  highlight_colors_[views::Button::STATE_PRESSED] = pressed_color;
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

void TabCloseButton::OnMouseMoved(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseMoved(event);
}

void TabCloseButton::OnMouseReleased(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseReleased(event);
}

void TabCloseButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

const char* TabCloseButton::GetClassName() const {
  return "TabCloseButton";
}

void TabCloseButton::Layout() {
  ImageButton::Layout();
  if (focus_ring()) {
    SkPath path;
    path.addOval(gfx::RectToSkRect(GetMirroredRect(GetContentsBounds())));
    focus_ring()->SetPath(path);
  }
}

gfx::Size TabCloseButton::CalculatePreferredSize() const {
  int width = GetWidth();
  gfx::Size size(width, width);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void TabCloseButton::PaintButtonContents(gfx::Canvas* canvas) {
  ButtonState button_state = state();
  // Draw the background circle highlight.
  if (button_state != views::Button::STATE_NORMAL)
    DrawHighlight(canvas, button_state);
  DrawCloseGlyph(canvas, button_state);
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

bool TabCloseButton::GetHitTestMask(gfx::Path* mask) const {
  // We need to define this so hit-testing won't include the border region.
  mask->addRect(gfx::RectToSkRect(GetMirroredRect(GetContentsBounds())));
  return true;
}

void TabCloseButton::DrawHighlight(gfx::Canvas* canvas, ButtonState state) {
  gfx::Path path;
  gfx::Point center = GetContentsBounds().CenterPoint();
  path.setFillType(SkPath::kEvenOdd_FillType);
  path.addCircle(center.x(), center.y(), GetWidth() / 2);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(highlight_colors_[state]);
  canvas->DrawPath(path, flags);
}

void TabCloseButton::DrawCloseGlyph(gfx::Canvas* canvas, ButtonState state) {
  cc::PaintFlags flags;
  constexpr float kStrokeWidth = 1.5f;
  float touch_scale = float{GetWidth()} / kGlyphWidth;
  float size = (kGlyphWidth - 8) * touch_scale - kStrokeWidth;
  gfx::RectF glyph_bounds(GetContentsBounds());
  glyph_bounds.ClampToCenteredSize(gfx::SizeF(size, size));
  flags.setAntiAlias(true);
  flags.setStrokeWidth(kStrokeWidth);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setColor(icon_colors_[state]);
  canvas->DrawLine(glyph_bounds.origin(), glyph_bounds.bottom_right(), flags);
  canvas->DrawLine(glyph_bounds.bottom_left(), glyph_bounds.top_right(), flags);
}
