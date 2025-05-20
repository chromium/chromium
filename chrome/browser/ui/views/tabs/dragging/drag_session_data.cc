// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"

#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/tabs/public/tab_group.h"

TabDragData::TabDragData(TabDragContext* source_context, TabSlotView* view)
    : source_model_index(source_context->GetIndexOf(view)),
      view_type(view->GetTabSlotViewType()) {
  source_model_index = source_context->GetIndexOf(view);
  if (source_model_index.has_value()) {
    contents = source_context->GetTabStripModel()->GetWebContentsAt(
        source_model_index.value());
    pinned = source_context->IsTabPinned(static_cast<Tab*>(view));
  }
  std::optional<tab_groups::TabGroupId> tab_group_id = view->group();
  if (tab_group_id.has_value()) {
    tab_group_data = TabDragData::TabGroupData{
        tab_group_id.value(), *source_context->GetTabStripModel()
                                   ->group_model()
                                   ->GetTabGroup(tab_group_id.value())
                                   ->visual_data()};
  }
}

TabDragData::TabDragData(const TabDragData&) = default;
TabDragData& TabDragData::operator=(const TabDragData&) = default;
TabDragData::~TabDragData() = default;
TabDragData::TabDragData(TabDragData&&) = default;

DragSessionData::DragSessionData() = default;
DragSessionData::DragSessionData(const DragSessionData&) = default;
DragSessionData& DragSessionData::operator=(const DragSessionData&) = default;
DragSessionData::~DragSessionData() = default;
DragSessionData::DragSessionData(DragSessionData&&) = default;
