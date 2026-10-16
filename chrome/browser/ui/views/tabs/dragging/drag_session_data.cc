// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "ui/views/view.h"

namespace {
std::optional<int> ViewModelIndex(TabDragContext* context, views::View* view) {
  content::WebContents* contents = context->GetContentsForTab(view);
  if (!contents) {
    return std::nullopt;
  }
  int index = context->GetTabStripModel()->GetIndexOfWebContents(contents);
  return index != TabStripModel::kNoTab ? std::make_optional(index)
                                        : std::nullopt;
}
}  // namespace

TabDragData::TabDragData(TabDragContext* source_context, views::View* view)
    : source_model_index(ViewModelIndex(source_context, view)) {
  std::optional<tab_groups::TabGroupId> tab_group_id;
  if (source_model_index.has_value()) {
    contents = source_context->GetContentsForTab(view);
    pinned = source_context->GetTabStripModel()->IsTabPinned(
        source_model_index.value());
    tab_group_id = source_context->GetTabStripModel()->GetTabGroupForTab(
        source_model_index.value());
  } else {
    tab_group_id = source_context->GetTabGroupForHeader(view);
    is_group_header = tab_group_id.has_value();
  }
  if (tab_group_id.has_value()) {
    const TabGroup* group =
        source_context->GetTabStripModel()->group_model()->GetTabGroup(
            tab_group_id.value());
    int index_in_group = source_model_index.has_value()
                             ? source_model_index.value() -
                                   static_cast<int>(group->ListTabs().start())
                             : 0;
    tab_group_data = TabDragData::TabGroupData{
        tab_group_id.value(), *group->visual_data(), index_in_group};
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
