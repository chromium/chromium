// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"

#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/metadata/metadata_impl_macros.h"

TabContainer::~TabContainer() {
  RemoveAllChildViews();
}

Tab* TabContainer::AddTab(std::unique_ptr<Tab> tab, int model_index) {
  absl::optional<tab_groups::TabGroupId> group = tab->group();
  return AddChildViewAt(
      std::move(tab), GetViewInsertionIndex(group, absl::nullopt, model_index));
}

void TabContainer::MoveTab(Tab* tab, int from_model_index, int to_model_index) {
  ReorderChildView(tab, GetViewInsertionIndex(tab->group(), from_model_index,
                                              to_model_index));
}

void TabContainer::MoveGroupHeader(TabGroupHeader* group_header,
                                   int first_tab_model_index) {
  const int header_index = GetIndexOf(group_header);
  const int first_tab_view_index =
      GetIndexOf(tab_strip_->tab_at(first_tab_model_index));

  // The header should be just before the first tab. If it isn't, reorder the
  // header such that it is. Note that the index to reorder to is different
  // depending on whether the header is before or after the tab, since the
  // header itself occupies an index.
  if (header_index < first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index - 1);
  if (header_index > first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index);
}

int TabContainer::GetViewInsertionIndex(
    absl::optional<tab_groups::TabGroupId> group,
    absl::optional<int> from_model_index,
    int to_model_index) const {
  // -1 is treated a sentinel value to indicate a tab is newly added to the
  // beginning of the tab strip.
  if (to_model_index < 0)
    return 0;

  // If to_model_index is beyond the end of the tab strip, then the tab is newly
  // added to the end of the tab strip. In that case we can just return one
  // beyond the view index of the last existing tab.
  if (to_model_index >= tab_strip_->GetTabCount())
    return (
        tab_strip_->GetTabCount()
            ? GetIndexOf(tab_strip_->tab_at(tab_strip_->GetTabCount() - 1)) + 1
            : 0);

  // If there is no from_model_index, then the tab is newly added in the middle
  // of the tab strip. In that case we treat it as coming from the end of the
  // tab strip, since new views are ordered at the end by default.
  if (!from_model_index.has_value())
    from_model_index = tab_strip_->GetTabCount();

  DCHECK_NE(to_model_index, from_model_index.value());

  // Since we don't have an absolute mapping from model index to view index, we
  // anchor on the last known view index at the given to_model_index.
  Tab* other_tab = tab_strip_->tab_at(to_model_index);
  int other_view_index = GetIndexOf(other_tab);

  if (other_view_index <= 0)
    return 0;

  // When moving to the right, just use the anchor index because the tab will
  // replace that position in both the model and the view. This happens because
  // the tab itself occupies a lower index that the other tabs will shift into.
  if (to_model_index > from_model_index.value())
    return other_view_index;

  // When moving to the left, the tab may end up on either the left or right
  // side of a group header, depending on if it's in that group. This affects
  // its view index but not its model index, so we adjust the former only.
  if (other_tab->group().has_value() && other_tab->group() != group)
    return other_view_index - 1;

  return other_view_index;
}

BEGIN_METADATA(TabContainer, views::View)
END_METADATA
