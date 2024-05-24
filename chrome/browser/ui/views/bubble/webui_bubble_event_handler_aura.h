// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_EVENT_HANDLER_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_EVENT_HANDLER_AURA_H_

#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

// Simple drag handling for non platform window backed aura widgets.
class WebUIBubbleEventHandlerAura : public ui::EventHandler {
 public:
  WebUIBubbleEventHandlerAura();
  WebUIBubbleEventHandlerAura(const WebUIBubbleEventHandlerAura&) = delete;
  WebUIBubbleEventHandlerAura& operator=(const WebUIBubbleEventHandlerAura&) =
      delete;
  ~WebUIBubbleEventHandlerAura() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // True if `original_bounds` and `new_bounds` belong to the same display.
  // Input must be in screen coordinates.
  bool IsInSameDisplay(const gfx::Rect& original_bounds,
                       const gfx::Rect& new_bounds);

  void ProcessLocatedEvent(ui::LocatedEvent* event);

  // Tracks whether we are currently performing a bubble drag.
  bool dragging_ = false;

  // Window bounds when the drag starts in screen coordinates.
  gfx::Rect initial_bounds_;

  // Pointer offset from the window position when the drag starts.
  gfx::Vector2d initial_offset_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_EVENT_HANDLER_AURA_H_
