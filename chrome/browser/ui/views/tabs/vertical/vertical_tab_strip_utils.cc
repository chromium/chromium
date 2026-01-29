// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_utils.h"

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

gfx::Rect GetVerticalTabStripViewTargetBounds(const views::View* view) {
  CHECK(view);

  const views::View* const parent = view->parent();
  const auto has_animating_layout_manager = [](const views::View* container) {
    // New clients of `TabCollectionAnimatingLayoutManager` should be added to
    // this list as usage expands.
    return views::IsViewClass<VerticalPinnedTabContainerView>(container) ||
           views::IsViewClass<VerticalUnpinnedTabContainerView>(container) ||
           views::IsViewClass<VerticalTabGroupView>(container);
  };
  if (!parent || !has_animating_layout_manager(parent)) {
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
