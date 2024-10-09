// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/overlay_agent_views.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/render_text.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

namespace ui_devtools {

namespace {

void DrawRulerText(const std::u16string& utf16_text,
                   const gfx::Point& p,
                   gfx::Canvas* canvas,
                   gfx::RenderText* render_text_) {
  render_text_->SetText(utf16_text);
  render_text_->SetColor(SK_ColorRED);
  const gfx::Rect text_rect(gfx::Rect(p, render_text_->GetStringSize()));
  canvas->FillRect(text_rect, SK_ColorWHITE, SkBlendMode::kColor);
  render_text_->SetDisplayRect(text_rect);
  render_text_->Draw(canvas);
}

void DrawRulers(const gfx::Rect& screen_bounds,
                gfx::Canvas* canvas,
                gfx::RenderText* render_text_) {
  // Top horizontal ruler from left to right.
  canvas->Draw1pxLine(gfx::PointF(0.0f, 0.0f),
                      gfx::PointF(screen_bounds.right(), 0.0f),
                      SK_ColorMAGENTA);

  // Left vertical ruler from top to bottom.
  canvas->Draw1pxLine(gfx::PointF(0.0f, 0.0f),
                      gfx::PointF(0.0f, screen_bounds.bottom()),
                      SK_ColorMAGENTA);

  int short_stroke = 5;
  int mid_stroke = 7;
  int long_stroke = 10;
  int gap_between_strokes = 5;
  int gap_between_mid_stroke = 25;
  int gap_between_long_stroke = 100;

  // Draw top horizontal ruler.
  for (int x = gap_between_strokes; x < screen_bounds.right();
       x += gap_between_strokes) {
    if (x % gap_between_long_stroke == 0) {
      canvas->Draw1pxLine(gfx::PointF(x, 0.0f), gfx::PointF(x, long_stroke),
                          SK_ColorMAGENTA);
      // Draw ruler marks.
      std::u16string utf16_text = base::UTF8ToUTF16(base::NumberToString(x));
      DrawRulerText(utf16_text, gfx::Point(x + 2, long_stroke), canvas,
                    render_text_);

    } else if (x % gap_between_mid_stroke == 0) {
      canvas->Draw1pxLine(gfx::PointF(x, 0.0f), gfx::PointF(x, mid_stroke),
                          SK_ColorMAGENTA);
    } else {
      canvas->Draw1pxLine(gfx::PointF(x, 0.0f), gfx::PointF(x, short_stroke),
                          SK_ColorMAGENTA);
    }
  }

  // Draw left vertical ruler.
  for (int y = 0; y < screen_bounds.bottom(); y += gap_between_strokes) {
    if (y % gap_between_long_stroke == 0) {
      canvas->Draw1pxLine(gfx::PointF(0.0f, y), gfx::PointF(long_stroke, y),
                          SK_ColorMAGENTA);
      // Draw ruler marks.
      std::u16string utf16_text = base::UTF8ToUTF16(base::NumberToString(y));
      DrawRulerText(utf16_text, gfx::Point(short_stroke + 1, y + 2), canvas,
                    render_text_);
    } else {
      canvas->Draw1pxLine(gfx::PointF(0.0f, y), gfx::PointF(short_stroke, y),
                          SK_ColorMAGENTA);
    }
  }
}

// Draw width() x height() of a rectangle if not empty. Otherwise, draw either
// width() or height() if any of them is not empty.
void DrawSizeOfRectangle(const gfx::Rect& hovered_rect,
                         const RectSide drawing_side,
                         gfx::Canvas* canvas,
                         gfx::RenderText* render_text_) {
  std::u16string utf16_text;
  const std::string unit = "dp";

  if (!hovered_rect.IsEmpty()) {
    utf16_text = base::UTF8ToUTF16(hovered_rect.size().ToString() + unit);
  } else if (hovered_rect.height()) {
    // Draw only height() if height() is not empty.
    utf16_text =
        base::UTF8ToUTF16(base::NumberToString(hovered_rect.height()) + unit);
  } else if (hovered_rect.width()) {
    // Draw only width() if width() is not empty.
    utf16_text =
        base::UTF8ToUTF16(base::NumberToString(hovered_rect.width()) + unit);
  } else {
    // If both width() and height() are empty, canvas won't draw size.
    return;
  }
  render_text_->SetText(std::move(utf16_text));
  render_text_->SetColor(SK_ColorRED);

  const gfx::Size& text_size = render_text_->GetStringSize();
  gfx::Rect text_rect;
  if (drawing_side == RectSide::LEFT_SIDE) {
    const gfx::Point text_left_side(
        hovered_rect.x() + 1,
        hovered_rect.height() / 2 - text_size.height() / 2 + hovered_rect.y());
    text_rect = gfx::Rect(text_left_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::RIGHT_SIDE) {
    const gfx::Point text_right_side(
        hovered_rect.right() - 1 - text_size.width(),
        hovered_rect.height() / 2 - text_size.height() / 2 + hovered_rect.y());
    text_rect = gfx::Rect(text_right_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::TOP_SIDE) {
    const gfx::Point text_top_side(
        hovered_rect.x() + hovered_rect.width() / 2 - text_size.width() / 2,
        hovered_rect.y() + 1);
    text_rect = gfx::Rect(text_top_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::BOTTOM_SIDE) {
    const gfx::Point text_top_side(
        hovered_rect.x() + hovered_rect.width() / 2 - text_size.width() / 2,
        hovered_rect.bottom() - 1 - text_size.height());
    text_rect = gfx::Rect(text_top_side,
                          gfx::Size(text_size.width(), text_size.height()));
  }
  canvas->FillRect(text_rect, SK_ColorWHITE, SkBlendMode::kColor);
  render_text_->SetDisplayRect(text_rect);
  render_text_->Draw(canvas);
}

void DrawRectGuideLinesOnCanvas(const gfx::Rect& screen_bounds,
                                const gfx::RectF& rect_f,
                                cc::PaintFlags flags,
                                gfx::Canvas* canvas) {
  // Top horizontal dotted line from left to right.
  canvas->DrawLine(gfx::PointF(0.0f, rect_f.y()),
                   gfx::PointF(screen_bounds.right(), rect_f.y()), flags);

  // Bottom horizontal dotted line from left to right.
  canvas->DrawLine(gfx::PointF(0.0f, rect_f.bottom()),
                   gfx::PointF(screen_bounds.right(), rect_f.bottom()), flags);

  // Left vertical dotted line from top to bottom.
  canvas->DrawLine(gfx::PointF(rect_f.x(), 0.0f),
                   gfx::PointF(rect_f.x(), screen_bounds.bottom()), flags);

  // Right vertical dotted line from top to bottom.
  canvas->DrawLine(gfx::PointF(rect_f.right(), 0.0f),
                   gfx::PointF(rect_f.right(), screen_bounds.bottom()), flags);
}

void DrawSizeWithAnyBounds(float x1,
                           float y1,
                           float x2,
                           float y2,
                           RectSide side,
                           gfx::Canvas* canvas,
                           gfx::RenderText* render_text) {
  if (x2 > x1 || y2 > y1) {
    DrawSizeOfRectangle(gfx::Rect(x1, y1, x2 - x1, y2 - y1), side, canvas,
                        render_text);
  } else {
    DrawSizeOfRectangle(gfx::Rect(x2, y2, x1 - x2, y1 - y2), side, canvas,
                        render_text);
  }
}

void DrawR1ContainsR2(const gfx::RectF& pinned_rect_f,
                      const gfx::RectF& hovered_rect_f,
                      const cc::PaintFlags& flags,
                      gfx::Canvas* canvas,
                      gfx::RenderText* render_text) {
  // Horizontal left distance line.
  float x1 = pinned_rect_f.x();
  float y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  float x2 = hovered_rect_f.x();
  float y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  // Horizontal right distance line.
  x1 = hovered_rect_f.right();
  y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  x2 = pinned_rect_f.right();
  y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  // Vertical top distance line.
  x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  y1 = pinned_rect_f.y();
  x2 = x1;
  y2 = hovered_rect_f.y();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);

  // Vertical bottom distance line.
  x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  y1 = hovered_rect_f.bottom();
  x2 = x1;
  y2 = pinned_rect_f.bottom();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1HorizontalFullLeftR2(const gfx::RectF& pinned_rect_f,
                                const gfx::RectF& hovered_rect_f,
                                const cc::PaintFlags& flags,
                                gfx::Canvas* canvas,
                                gfx::RenderText* render_text) {
  // Horizontal left distance line.
  float x1 = hovered_rect_f.right();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);
}

void DrawR1TopFullLeftR2(const gfx::RectF& pinned_rect_f,
                         const gfx::RectF& hovered_rect_f,
                         const cc::PaintFlags& flags,
                         gfx::Canvas* canvas_,
                         gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.x() + hovered_rect_f.width();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = hovered_rect_f.y() + hovered_rect_f.height() / 2;

  // Horizontal left dotted line.
  canvas_->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas_,
                        render_text);
  x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y1 = hovered_rect_f.y() + hovered_rect_f.height();
  x2 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y2 = pinned_rect_f.y();

  // Vertical left dotted line.
  canvas_->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas_,
                        render_text);
}

void DrawR1BottomFullLeftR2(const gfx::RectF& pinned_rect_f,
                            const gfx::RectF& hovered_rect_f,
                            const cc::PaintFlags& flags,
                            gfx::Canvas* canvas,
                            gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.right();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = y1;

  // Horizontal left distance line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y1 = pinned_rect_f.bottom();
  x2 = x1;
  y2 = hovered_rect_f.y();

  // Vertical left distance line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1TopPartialLeftR2(const gfx::RectF& pinned_rect_f,
                            const gfx::RectF& hovered_rect_f,
                            const cc::PaintFlags& flags,
                            gfx::Canvas* canvas,
                            gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  float y1 = hovered_rect_f.bottom();
  float x2 = x1;
  float y2 = pinned_rect_f.y();

  // Vertical left dotted line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1BottomPartialLeftR2(const gfx::RectF& pinned_rect_f,
                               const gfx::RectF& hovered_rect_f,
                               const cc::PaintFlags& flags,
                               gfx::Canvas* canvas,
                               gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  float y1 = pinned_rect_f.bottom();
  float x2 = x1;
  float y2 = hovered_rect_f.y();

  // Vertical left dotted line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1IntersectsR2(const gfx::RectF& pinned_rect_f,
                        const gfx::RectF& hovered_rect_f,
                        const cc::PaintFlags& flags,
                        gfx::Canvas* canvas,
                        gfx::RenderText* render_text) {
  // Vertical dotted line for the top side of the pinned rectangle
  float x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  float y1 = pinned_rect_f.y();
  float x2 = x1;
  float y2 = hovered_rect_f.y();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);

  // Vertical dotted line for the bottom side of the pinned rectangle
  x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  y1 = pinned_rect_f.bottom();
  x2 = x1;
  y2 = hovered_rect_f.bottom();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);

  // Horizontal dotted line for the left side of the pinned rectangle
  x1 = pinned_rect_f.x();
  y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  x2 = hovered_rect_f.x();
  y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  // Horizontal dotted line for the right side of the pinned rectangle
  x1 = pinned_rect_f.right();
  y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  x2 = hovered_rect_f.right();
  y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawSizeWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);
}

}  // namespace

OverlayAgentViews::OverlayAgentViews(DOMAgent* dom_agent)
    : OverlayAgent(dom_agent),
      show_size_on_canvas_(false),
      highlight_rect_config_(HighlightRectsConfiguration::NO_DRAW) {}

OverlayAgentViews::~OverlayAgentViews() = default;

void OverlayAgentViews::SetPinnedNodeId(int node_id) {
  pinned_id_ = node_id;
  frontend()->nodeHighlightRequested(pinned_id_);
  HighlightNode(pinned_id_, true /* show_size */);
}

protocol::Response OverlayAgentViews::setInspectMode(
    const protocol::String& in_mode,
    protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  pinned_id_ = 0;
  if (in_mode.compare("searchForNode") == 0) {
    InstallPreTargetHandler();
  } else if (in_mode.compare("none") == 0) {
    RemovePreTargetHandler();
  }
  return protocol::Response::Success();
}

protocol::Response OverlayAgentViews::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
    protocol::Maybe<int> node_id) {
  return HighlightNode(node_id.value());
}

protocol::Response OverlayAgentViews::hideHighlight() {
  if (layer_for_highlighting_ && layer_for_highlighting_->visible())
    layer_for_highlighting_->SetVisible(false);
  return protocol::Response::Success();
}

void OverlayAgentViews::ShowDistancesInHighlightOverlay(int pinned_id,
                                                        int element_id) {
  UIElement* element_r1 = dom_agent()->GetElementFromNodeId(pinned_id);
  UIElement* element_r2 = dom_agent()->GetElementFromNodeId(element_id);
  if (!element_r1 || !element_r2)
    return;

  const std::pair<gfx::NativeWindow, gfx::Rect> pair_r2(
      element_r2->GetNodeWindowAndScreenBounds());
  const std::pair<gfx::NativeWindow, gfx::Rect> pair_r1(
      element_r1->GetNodeWindowAndScreenBounds());
#if BUILDFLAG(IS_APPLE)
  // TODO(lgrey): Explain this
  if (pair_r1.first != pair_r2.first) {
    pinned_id_ = 0;
    return;
  }
#endif
  gfx::Rect r2(pair_r2.second);
  gfx::Rect r1(pair_r1.second);
  pinned_rect_ = r1;

  is_swap_ = false;
  if (r1.x() > r2.x()) {
    is_swap_ = true;
    std::swap(r1, r2);
  }
  if (r1.Contains(r2)) {
    highlight_rect_config_ = HighlightRectsConfiguration::R1_CONTAINS_R2;
  } else if (r1.right() <= r2.x()) {
    if ((r1.y() <= r2.y() && r2.y() <= r1.bottom()) ||
        (r1.y() <= r2.bottom() && r2.bottom() <= r1.bottom()) ||
        (r2.y() <= r1.y() && r1.y() <= r2.bottom()) ||
        (r2.y() <= r1.bottom() && r1.bottom() <= r2.bottom())) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_HORIZONTAL_FULL_LEFT_R2;
    } else if (r1.bottom() <= r2.y()) {
      highlight_rect_config_ = HighlightRectsConfiguration::R1_TOP_FULL_LEFT_R2;
    } else if (r1.y() >= r2.bottom()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_BOTTOM_FULL_LEFT_R2;
    }
  } else if (r1.x() <= r2.x() && r2.x() <= r1.right()) {
    if (r1.bottom() <= r2.y()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_TOP_PARTIAL_LEFT_R2;
    } else if (r1.y() >= r2.bottom()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_BOTTOM_PARTIAL_LEFT_R2;
    } else if (r1.Intersects(r2)) {
      highlight_rect_config_ = HighlightRectsConfiguration::R1_INTERSECTS_R2;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  } else {
    highlight_rect_config_ = HighlightRectsConfiguration::NO_DRAW;
  }
}

protocol::Response OverlayAgentViews::HighlightNode(int node_id,
                                                    bool show_size) {
  UIElement* element = dom_agent()->GetElementFromNodeId(node_id);
  if (!element)
    return protocol::Response::ServerError("No node found with that id");

  if (element->type() == UIElementType::ROOT)
    return protocol::Response::ServerError("Cannot highlight root node.");

  if (!layer_for_highlighting_) {
    layer_for_highlighting_ =
        std::make_unique<ui::Layer>(ui::LayerType::LAYER_TEXTURED);
    layer_for_highlighting_->SetName("HighlightingLayer");
    layer_for_highlighting_->set_delegate(this);
    layer_for_highlighting_->SetFillsBoundsOpaquely(false);
  }

  highlight_rect_config_ = HighlightRectsConfiguration::NO_DRAW;
  show_size_on_canvas_ = show_size;
  layer_for_highlighting_->SetVisible(
      UpdateHighlight(element->GetNodeWindowAndScreenBounds()));
  return protocol::Response::Success();
}

void OverlayAgentViews::OnMouseEvent(ui::MouseEvent* event) {
  // Make sure the element tree has been populated before processing
  // mouse events.
  if (!dom_agent()->element_root())
    return;

  // Show parent of the pinned element with id |pinned_id_| when mouse scrolls
  // up. If parent exists, highlight and re-pin parent element.
  if (event->type() == ui::EventType::kMousewheel && pinned_id_) {
    const ui::MouseWheelEvent* mouse_event =
        static_cast<ui::MouseWheelEvent*>(event);
    DCHECK(mouse_event);
    if (mouse_event->y_offset() > 0) {
      const int parent_node_id = dom_agent()->GetParentIdOfNodeId(pinned_id_);
      if (parent_node_id)
        SetPinnedNodeId(parent_node_id);
      event->SetHandled();
    } else if (mouse_event->y_offset() < 0) {
      // TODO(thanhph): discuss behaviours when mouse scrolls down.
    }
    return;
  }

  // Find node id of element whose bounds contain the mouse pointer location.
  int element_id = FindElementIdTargetedByPoint(event);
  if (!element_id)
    return;

#if defined(USE_AURA)
  aura::Window* target = static_cast<aura::Window*>(event->target());
  bool active_window = ::wm::IsActiveWindow(
      target->GetRootWindow()->GetEventHandlerForPoint(event->root_location()));
#else
  bool active_window = true;
#endif
  if (pinned_id_ == element_id && active_window) {
    event->SetHandled();
    return;
  }

  // Pin the hover element on click.
  if (event->type() == ui::EventType::kMousePressed) {
    if (active_window)
      event->SetHandled();
    SetPinnedNodeId(element_id);
  } else if (pinned_id_) {
    // If hovering with a pinned element, then show distances between the pinned
    // element and the hover element.
    HighlightNode(element_id, false /* show_size */);
    ShowDistancesInHighlightOverlay(pinned_id_, element_id);
  } else {
    // Display only guidelines if hovering without a pinned element.
    frontend()->nodeHighlightRequested(element_id);
    HighlightNode(element_id, false /* show_size */);
  }
}

void OverlayAgentViews::OnKeyEvent(ui::KeyEvent* event) {
  if (!dom_agent()->element_root())
    return;

  // Exit inspect mode by pressing ESC key.
  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    RemovePreTargetHandler();
    if (pinned_id_) {
      frontend()->inspectNodeRequested(pinned_id_);
      HighlightNode(pinned_id_, true /* show_size */);
    }
    // Unpin element.
    pinned_id_ = 0;
  }
}

void OverlayAgentViews::OnPaintLayer(const ui::PaintContext& context) {
  const gfx::Rect& screen_bounds(layer_for_highlighting_->bounds());
  ui::PaintRecorder recorder(context, screen_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();
  // Convert the hovered rect from screen coordinates to layer coordinates.
  gfx::RectF hovered_rect_f(hovered_rect_);
  hovered_rect_f.Offset(-layer_for_highlighting_screen_offset_);

  cc::PaintFlags flags;
  flags.setStrokeWidth(1.0f);
  flags.setColor(SK_ColorBLUE);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  constexpr SkScalar intervals[] = {1.f, 4.f};
  flags.setPathEffect(cc::PathEffect::MakeDash(intervals, 2, 0));

  if (!render_text_)
    render_text_ = gfx::RenderText::CreateRenderText();
  DrawRulers(screen_bounds, canvas, render_text_.get());

  // Display guide lines if |highlight_rect_config_| is NO_DRAW.
  if (highlight_rect_config_ == HighlightRectsConfiguration::NO_DRAW) {
    hovered_rect_f.Inset(gfx::InsetsF(-1));
    DrawRectGuideLinesOnCanvas(screen_bounds, hovered_rect_f, flags, canvas);
    // Draw |hovered_rect_f| bounds.
    flags.setPathEffect(nullptr);
    canvas->DrawRect(hovered_rect_f, flags);

    // Display size of the rectangle after mouse click.
    if (show_size_on_canvas_) {
      DrawSizeOfRectangle(gfx::ToNearestRect(hovered_rect_f),
                          RectSide::BOTTOM_SIDE, canvas, render_text_.get());
    }
    return;
  }
  flags.setPathEffect(nullptr);
  flags.setColor(SK_ColorBLUE);

  // Convert the pinned rect from screen coordinates to layer coordinates.
  gfx::RectF pinned_rect_f(pinned_rect_);
  pinned_rect_f.Offset(-layer_for_highlighting_screen_offset_);

  // Draw |pinned_rect_f| bounds in blue.
  canvas->DrawRect(pinned_rect_f, flags);

  // Draw |hovered_rect_f| bounds in green.
  flags.setColor(SK_ColorGREEN);
  canvas->DrawRect(hovered_rect_f, flags);

  // Draw distances in red colour.
  flags.setPathEffect(nullptr);
  flags.setColor(SK_ColorRED);

  // Make sure |pinned_rect_f| stays on the right or below of |hovered_rect_f|.
  if (pinned_rect_f.x() < hovered_rect_f.x() ||
      (pinned_rect_f.x() == hovered_rect_f.x() &&
       pinned_rect_f.y() < hovered_rect_f.y())) {
    std::swap(pinned_rect_f, hovered_rect_f);
  }

  switch (highlight_rect_config_) {
    case HighlightRectsConfiguration::R1_CONTAINS_R2:
      DrawR1ContainsR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                       render_text_.get());
      return;
    case HighlightRectsConfiguration::R1_HORIZONTAL_FULL_LEFT_R2:
      DrawR1HorizontalFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                                 render_text_.get());
      return;
    case HighlightRectsConfiguration::R1_TOP_FULL_LEFT_R2:
      DrawR1TopFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                          render_text_.get());

      // Draw 4 guide lines along distance lines.
      flags.setPathEffect(cc::PathEffect::MakeDash(intervals, 2, 0));

      // Bottom horizontal dotted line from left to right.
      canvas->DrawLine(
          gfx::PointF(0.0f, hovered_rect_f.bottom()),
          gfx::PointF(screen_bounds.right(), hovered_rect_f.bottom()), flags);

      // Right vertical dotted line from top to bottom.
      canvas->DrawLine(
          gfx::PointF(hovered_rect_f.right(), 0.0f),
          gfx::PointF(hovered_rect_f.right(), screen_bounds.bottom()), flags);

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(gfx::PointF(0.0f, pinned_rect_f.y()),
                       gfx::PointF(screen_bounds.right(), pinned_rect_f.y()),
                       flags);

      // Left vertical dotted line from top to bottom.
      canvas->DrawLine(gfx::PointF(pinned_rect_f.x(), 0.0f),
                       gfx::PointF(pinned_rect_f.x(), screen_bounds.bottom()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_BOTTOM_FULL_LEFT_R2:
      DrawR1BottomFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                             render_text_.get());

      // Draw 2 guide lines along distance lines.
      flags.setPathEffect(cc::PathEffect::MakeDash(intervals, 2, 0));

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(
          gfx::PointF(0.0f, pinned_rect_f.bottom()),
          gfx::PointF(screen_bounds.right(), pinned_rect_f.bottom()), flags);

      // Left vertical dotted line from top to bottom.
      canvas->DrawLine(gfx::PointF(pinned_rect_f.x(), 0.0f),
                       gfx::PointF(pinned_rect_f.x(), screen_bounds.bottom()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_TOP_PARTIAL_LEFT_R2:
      DrawR1TopPartialLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                             render_text_.get());

      // Draw 1 guide line along distance lines.
      flags.setPathEffect(cc::PathEffect::MakeDash(intervals, 2, 0));

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(gfx::PointF(0.0f, pinned_rect_f.y()),
                       gfx::PointF(screen_bounds.right(), pinned_rect_f.y()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_BOTTOM_PARTIAL_LEFT_R2:
      DrawR1BottomPartialLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                                render_text_.get());
      return;
    case HighlightRectsConfiguration::R1_INTERSECTS_R2:
      DrawR1IntersectsR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                         render_text_.get());
      // Draw 4 guide line along distance lines.
      flags.setPathEffect(cc::PathEffect::MakeDash(intervals, 2, 0));

      DrawRectGuideLinesOnCanvas(screen_bounds, hovered_rect_f, flags, canvas);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

bool OverlayAgentViews::UpdateHighlight(
    const std::pair<gfx::NativeWindow, gfx::Rect>& window_and_bounds) {
  if (window_and_bounds.second.IsEmpty()) {
    hovered_rect_.SetRect(0, 0, 0, 0);
    return false;
  }
  ui::Layer* root_layer = nullptr;
#if BUILDFLAG(IS_APPLE)
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window_and_bounds.first);
  root_layer = widget->GetLayer();
  layer_for_highlighting_screen_offset_ =
      widget->GetContentsView()->GetBoundsInScreen().OffsetFromOrigin();
#else
  gfx::NativeWindow root = window_and_bounds.first->GetRootWindow();
  root_layer = root->layer();
  layer_for_highlighting_screen_offset_ =
      root->GetBoundsInScreen().OffsetFromOrigin();
#endif  // BUILDFLAG(IS_APPLE)
  DCHECK(root_layer);

  layer_for_highlighting_->SetBounds(root_layer->bounds());
  layer_for_highlighting_->SchedulePaint(root_layer->bounds());

  if (root_layer != layer_for_highlighting_->parent())
    root_layer->Add(layer_for_highlighting_.get());
  else
    root_layer->StackAtTop(layer_for_highlighting_.get());

  hovered_rect_ = window_and_bounds.second;
  return true;
}

}  // namespace ui_devtools
