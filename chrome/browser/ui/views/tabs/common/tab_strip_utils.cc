// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

gfx::Rect GetTabStripViewTargetBounds(const views::View* view) {
  CHECK(view);

  const views::View* const parent = view->parent();
  const auto has_animating_layout_manager = [](const views::View* container) {
    // New clients of `TabCollectionAnimatingLayoutManager` should be added to
    // this list as usage expands.
    return views::IsViewClass<PinnedTabContainerView>(container) ||
           views::IsViewClass<UnpinnedTabContainerView>(container) ||
           views::IsViewClass<TabGroupView>(container);
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

TabStripView* GetTabStripView(views::View* view) {
  for (views::View* v = view->parent(); v; v = v->parent()) {
    if (auto* tab_strip = views::AsViewClass<TabStripView>(v)) {
      return tab_strip;
    }
  }
  return nullptr;
}
