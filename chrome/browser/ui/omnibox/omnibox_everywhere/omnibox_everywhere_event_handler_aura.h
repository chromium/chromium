// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace omnibox_everywhere {

class OmniboxEverywhereUIManager;

// Intercepts mouse events on the Omnibox Everywhere native window.
// Triggers window dragging via RunMoveLoop() when dragging on background
// regions, while passing mouse presses through to WebContents for text
// selection and UI clicks.
class OmniboxEverywhereEventHandlerAura : public ui::EventHandler {
 public:
  explicit OmniboxEverywhereEventHandlerAura(
      OmniboxEverywhereUIManager& ui_manager);
  OmniboxEverywhereEventHandlerAura(const OmniboxEverywhereEventHandlerAura&) =
      delete;
  OmniboxEverywhereEventHandlerAura& operator=(
      const OmniboxEverywhereEventHandlerAura&) = delete;
  ~OmniboxEverywhereEventHandlerAura() override;

  bool IsPointInDraggableRegion(const gfx::Point& point) const;

  // Ends any pending drag hold, including its data-drag suppression. The owner
  // must call this whenever this handler may stop seeing the widget's mouse
  // events (e.g. when the widget is hidden or destroyed), since the handler
  // would otherwise never see the release or capture loss that ends the hold.
  void ResetDragState();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  const raw_ref<OmniboxEverywhereUIManager> ui_manager_;

  // The location of the mouse-pressed event that is eligible to trigger a
  // drag. Null if a drag is not eligible (e.g. a mouse-released event happened
  // recently).
  std::optional<gfx::Point> drag_init_point_screen_;

  // Suppresses renderer-initiated data drags while a left mouse press is held
  // on a draggable region waiting to cross the window-drag threshold.
  base::ScopedClosureRunner window_move_drag_suppression_;

  base::WeakPtrFactory<OmniboxEverywhereEventHandlerAura> weak_factory_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_
