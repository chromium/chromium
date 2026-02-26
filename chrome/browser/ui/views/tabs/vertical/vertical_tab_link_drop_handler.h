// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_LINK_DROP_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_LINK_DROP_HANDLER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"

class TabCollectionNode;
class TabStripModel;

// Handles calculating the drop index for link dragging in the vertical tab
// strip. This class encapsulates the logic for mapping a visual drop location
// over a tab collection node to a logical insertion index in the TabStripModel.
class VerticalTabLinkDropHandler {
 public:
  explicit VerticalTabLinkDropHandler(TabStripModel& tab_strip_model);
  ~VerticalTabLinkDropHandler();

  VerticalTabLinkDropHandler(const VerticalTabLinkDropHandler&) = delete;
  VerticalTabLinkDropHandler& operator=(const VerticalTabLinkDropHandler&) =
      delete;

  // Returns the DropIndex corresponding to a drag over `node` at the position
  // indicated by `position_hint`.
  std::optional<BrowserRootView::DropIndex> GetDropIndexForNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const;

 private:
  // Helper methods to calculate the drop index for specific node types.
  std::optional<BrowserRootView::DropIndex> GetDropIndexForTab(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const;
  std::optional<BrowserRootView::DropIndex> GetDropIndexForGroup(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const;
  std::optional<BrowserRootView::DropIndex> GetDropIndexForSplit(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const;
  std::optional<BrowserRootView::DropIndex> GetDropIndexForPinned() const;
  std::optional<BrowserRootView::DropIndex> GetDropIndexForUnpinned() const;

  const raw_ref<TabStripModel> tab_strip_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_LINK_DROP_HANDLER_H_
