// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WINDOW_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WINDOW_DRAG_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace views {
class Widget;
}  // namespace views

namespace omnibox_everywhere {

// Moves the Omnibox Everywhere window when the user drags a
// `-webkit-app-region: drag` area. It works from mouse events seen in
// `WebContentsDelegate::PreHandleMouseEvent()`, on every platform:
// - A left press in a draggable region arms a drag, and still reaches Blink so
//   clicks, focus, etc., work.
// - Once the press moves past the drag threshold, it posts a widget move-loop
//   and drops mouse moves until the drag ends.
class OmniboxEverywhereWindowDragController {
 public:
  class Delegate {
   public:
    virtual views::Widget* GetWindowDragWidget() = 0;
    // `point` is in the `WebContents`' main-frame coordinates.
    virtual bool IsPointInDraggableRegion(const gfx::Point& point) const = 0;
    // Returns false while UI that must block dragging (modals, context menus)
    // is open.
    virtual bool CanStartWindowDrag() const = 0;
    virtual bool IsWindowDragging() const = 0;
    virtual void OnWindowDragStarted() = 0;
    virtual void OnWindowDragEnded() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // `delegate` must outlive this controller.
  explicit OmniboxEverywhereWindowDragController(Delegate& delegate);
  OmniboxEverywhereWindowDragController(
      const OmniboxEverywhereWindowDragController&) = delete;
  OmniboxEverywhereWindowDragController& operator=(
      const OmniboxEverywhereWindowDragController&) = delete;
  ~OmniboxEverywhereWindowDragController();

  // Returns true if `event` should not reach the renderer.
  bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // Ends a pending drag and cancels a posted move loop. The owner must
  // call this whenever the widget may stop delivering mouse events (hidden,
  // deactivated, demoted or destroyed), since the release would never arrive.
  void Reset();

  bool has_pending_drag_for_testing() const {
    return drag_init_point_screen_.has_value();
  }

 private:
  bool HandleMouseDown(const blink::WebMouseEvent& event);
  bool HandleMouseMove(const blink::WebMouseEvent& event);
  bool HandleMouseUp(const blink::WebMouseEvent& event);
  bool PrepareForWindowDrag();
  void StartWindowMoveLoop(const gfx::Vector2d& drag_offset);

  const raw_ref<Delegate> delegate_;

  // Screen location of the left press that armed the drag.
  std::optional<gfx::Point> drag_init_point_screen_;

  // Suppresses renderer-initiated data drags while a left press is held on a
  // draggable region waiting to cross the drag threshold, and during the move
  // loop.
  base::ScopedClosureRunner window_move_drag_suppression_;

  base::WeakPtrFactory<OmniboxEverywhereWindowDragController>
      move_loop_weak_factory_{this};
  base::WeakPtrFactory<OmniboxEverywhereWindowDragController> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WINDOW_DRAG_CONTROLLER_H_
