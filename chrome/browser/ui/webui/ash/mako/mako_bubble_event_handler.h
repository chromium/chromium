// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_EVENT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_EVENT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

// By default WebUI bubbles are not draggable, this handler brings extra
// dragging support for Mako UI.
class MakoBubbleEventHandler : public ui::EventHandler {
 public:
  class Delegate {
   public:
    virtual const std::optional<SkRegion> GetDraggableRegion() = 0;
    virtual const gfx::Rect GetWidgetBoundsInScreen() = 0;
    virtual void SetWidgetBoundsConstrained(const gfx::Rect bounds) = 0;
  };

  struct InitialState {};
  struct DraggingState {
    gfx::Rect original_bounds_in_screen;
    gfx::Vector2d original_pointer_pos;
  };
  using State = absl::variant<InitialState, DraggingState>;

  explicit MakoBubbleEventHandler(Delegate* delegate);

  MakoBubbleEventHandler(const MakoBubbleEventHandler&) = delete;
  MakoBubbleEventHandler& operator=(const MakoBubbleEventHandler&) = delete;

  ~MakoBubbleEventHandler() override = default;

  // ui::EventHandler
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Test only
  State get_state_for_testing();
  void set_state_for_testing(State s);

 private:
  void ProcessPointerEvent(ui::LocatedEvent& event);

  raw_ptr<Delegate> delegate_;
  State state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_EVENT_HANDLER_H_
