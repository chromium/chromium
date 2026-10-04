// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"

#include <cmath>

#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "ui/events/event.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

gfx::Rect GetTabStripViewTargetBounds(const views::View* view) {
  CHECK(view);

  const views::View* const parent = view->parent();
  if (!parent || !parent->GetProperty(kHasAnimatingLayoutManagerKey)) {
    return view->bounds();
  }

  const auto* const layout_manager =
      static_cast<const TabCollectionAnimatingLayoutManager*>(
          parent->GetLayoutManager());
  CHECK(layout_manager);

  const views::ChildLayout* const view_layout =
      layout_manager->target_layout().GetLayoutFor(view);
  return view_layout ? view_layout->bounds : view->bounds();
}

TabStripView* GetTabStripView(views::View* view) {
  for (views::View* v = view->parent(); v; v = v->parent()) {
    if (auto* tab_strip = views::AsViewClass<TabStripView>(v)) {
      return tab_strip;
    }
  }
  return nullptr;
}

bool CanScrollAlongAxis(const views::View* view,
                        TabStripOrientation orientation,
                        const ui::GestureEvent& event) {
  const views::ScrollView* scroll_view = nullptr;
  for (const views::View* curr = view->parent(); curr; curr = curr->parent()) {
    if (const auto* sv = views::AsViewClass<views::ScrollView>(curr)) {
      scroll_view = sv;
      break;
    }
  }
  if (!scroll_view) {
    return false;
  }
  const float dx = std::abs(event.details().scroll_x_hint());
  const float dy = std::abs(event.details().scroll_y_hint());
  if (orientation == TabStripOrientation::kVertical) {
    return scroll_view->IsVerticalContentOverflowing() && dy >= dx;
  }
  return scroll_view->IsHorizontalContentOverflowing() && dx >= dy;
}
