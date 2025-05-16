// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAG_SESSION_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAG_SESSION_DATA_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace content {
class WebContents;
}

class TabSlotView;
class TabDragContext;

// Stores the data associated with a group header that is being dragged.
struct GroupDragData final {
  // The group that is being dragged.
  tab_groups::TabGroupId group;

  // The index of the tab within the group, if any, that was active when the
  // drag began. Defaults to 0 if the active tab was outside the group, as we
  // should fall back on activating the first tab during/after the drag.
  int active_tab_index_within_group;

  GroupDragData(tab_groups::TabGroupId group, int active_tab_index_within_group)
      : group(group),
        active_tab_index_within_group(active_tab_index_within_group) {}
};

// Stores the data associated with a single tab that is being dragged.
struct TabDragData final {
  TabDragData(TabDragContext* source_context, TabSlotView* view);
  TabDragData(const TabDragData&);
  TabDragData& operator=(const TabDragData&);
  ~TabDragData();
  TabDragData(TabDragData&&);

  // This is the index of the tab in `source_context_` when the drag
  // began. This is used to restore the previous state if the drag is aborted.
  // Nullopt if this is a group header.
  std::optional<int> source_model_index;

  TabSlotView::ViewType view_type;

  // The WebContents being dragged.
  raw_ptr<content::WebContents> contents = nullptr;

  // If attached this is the view in `attached_context_`.
  raw_ptr<TabSlotView> attached_view = nullptr;

  // Is the tab pinned?
  bool pinned = false;

  // Contains the information for the tab's group at the start of the drag.
  struct TabGroupData {
    tab_groups::TabGroupId group_id;
    tab_groups::TabGroupVisualData group_visual_data;
  };

  // The information on the group the tab was in at the start of the drag, or
  // nullopt if tab was not grouped.
  std::optional<TabGroupData> tab_group_data = std::nullopt;
};

// Stores the data for the tabs and (if applicable) group header being dragged.
// This is the core configuration for a tab drag session.
struct DragSessionData final {
  DragSessionData();
  DragSessionData(const DragSessionData&);
  DragSessionData& operator=(const DragSessionData&);
  ~DragSessionData();
  DragSessionData(DragSessionData&&);

  std::vector<TabDragData> tab_drag_data_;
  // Data related to the dragged tab group, if any. This is only set if the
  // drag originated from a group header, indicating that the entire group is
  // being dragged together.
  std::optional<GroupDragData> group_drag_data_ = std::nullopt;

  // Index of the source view in `tab_drag_data_`. This is the view that the
  // user started dragging.
  size_t source_view_index_ = std::numeric_limits<size_t>::max();

  std::optional<tab_groups::TabGroupId> group() const {
    return group_drag_data_.has_value()
               ? std::make_optional(group_drag_data_.value().group)
               : std::nullopt;
  }

  // Convenience for getting the TabDragData corresponding to the source view
  // that the user started dragging.
  const TabDragData* source_view_drag_data() const {
    return &(tab_drag_data_[source_view_index_]);
  }

  // Convenience for `source_view_drag_data()->contents`.
  content::WebContents* source_dragged_contents() const {
    return source_view_drag_data()->contents;
  }

  // Returns the number of Tab views currently dragging.
  // Excludes the TabGroupHeader view, if any.
  int num_dragging_tabs() const {
    int num_tabs = 0;
    for (const TabDragData& tab_drag_datum : tab_drag_data_) {
      if (tab_drag_datum.attached_view->GetTabSlotViewType() ==
          TabSlotView::ViewType::kTab) {
        num_tabs++;
      }
    }
    return num_tabs;
  }

  std::vector<TabSlotView*> attached_views() const {
    std::vector<TabSlotView*> attached_views;
    for (const TabDragData& tab_data : tab_drag_data_) {
      attached_views.push_back(tab_data.attached_view);
    }
    return attached_views;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAG_SESSION_DATA_H_
