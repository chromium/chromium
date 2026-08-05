// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_

#include <optional>

#include "base/memory/raw_ref.h"
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

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  const raw_ref<OmniboxEverywhereUIManager> ui_manager_;

  // The location of the mouse-pressed event that is eligible to trigger a
  // drag. Null if a drag is not eligible (e.g. a mouse-released event happened
  // recently).
  std::optional<gfx::Point> drag_init_point_screen_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_EVENT_HANDLER_AURA_H_
