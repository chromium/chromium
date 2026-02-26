// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_link_drop_handler.h"

#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace {

const TabGroup& TabGroupDataFromNode(const TabCollectionNode& node) {
  CHECK_EQ(node.type(), TabCollectionNode::Type::GROUP);
  const auto* collection = static_cast<const tabs::TabGroupTabCollection*>(
      std::get<const tabs::TabCollection*>(node.GetNodeData()));
  CHECK(collection);
  const auto* group_data = collection->GetTabGroup();
  CHECK(group_data);
  return *group_data;
}

}  // namespace

VerticalTabLinkDropHandler::VerticalTabLinkDropHandler(
    TabStripModel& tab_strip_model)
    : tab_strip_model_(tab_strip_model) {}

VerticalTabLinkDropHandler::~VerticalTabLinkDropHandler() = default;

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForNode(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) const {
  switch (node.type()) {
    case TabCollectionNode::Type::TAB:
      return GetDropIndexForTab(node, position_hint);
    case TabCollectionNode::Type::GROUP:
      return GetDropIndexForGroup(node, position_hint);
    case TabCollectionNode::Type::SPLIT:
      return GetDropIndexForSplit(node, position_hint);
    case TabCollectionNode::Type::PINNED:
      return GetDropIndexForPinned();
    case TabCollectionNode::Type::UNPINNED:
      return GetDropIndexForUnpinned();
    default:
      NOTREACHED();
  }
}

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForTab(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) const {
  const auto* tab = std::get<const tabs::TabInterface*>(node.GetNodeData());
  CHECK(tab);
  int index = tab_strip_model_->GetIndexOfTab(tab);

  if (!position_hint.has_value()) {
    return BrowserRootView::DropIndex{
        .index = index,
        .relative_to_index =
            BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex};
  }

  BrowserRootView::DropIndex::GroupInclusion group_inclusion =
      BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup;

  if (auto group_id = tab->GetGroup()) {
    const TabGroup* group =
        tab_strip_model_->group_model()->GetTabGroup(*group_id);
    if (position_hint == DragPositionHint::kAfter &&
        tab == group->GetLastTab()) {
      // If dropping after the last tab in the group, then don't include the
      // new tab in the group. Otherwise, there is ambiguity around which
      // group to insert into if there are consecutive groups.
      group_inclusion =
          BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup;
    } else {
      group_inclusion =
          BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup;
    }
  }

  if (position_hint == DragPositionHint::kAfter) {
    index++;
  }

  return BrowserRootView::DropIndex{
      .index = index,
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex,
      .group_inclusion = group_inclusion};
}

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForGroup(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) const {
  const auto& group = TabGroupDataFromNode(node);
  const bool is_collapsed = tab_strip_model_->group_model()
                                ->GetTabGroup(group.id())
                                ->visual_data()
                                ->is_collapsed();
  int index = static_cast<int>(position_hint == DragPositionHint::kAfter
                                   ? group.ListTabs().end()
                                   : group.ListTabs().start());
  if (is_collapsed) {
    return BrowserRootView::DropIndex{
        .index = index,
        .relative_to_index =
            BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex,
        .group_inclusion =
            BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup};
  }

  // If dropping after the header in an expanded group, then the new tab should
  // also be in the group. If before the header, then it shouldn't be in the
  // group.
  return BrowserRootView::DropIndex{
      .index = index,
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex,
      .group_inclusion =
          position_hint.has_value()
              ? BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup
              : BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup};
}

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForSplit(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) const {
  const TabCollectionNode* first_tab_node = node.children()[0].get();
  auto* tab_interface =
      std::get<const tabs::TabInterface*>(first_tab_node->GetNodeData());
  CHECK(tab_interface);
  int index = tab_strip_model_->GetIndexOfTab(tab_interface);
  if (position_hint == DragPositionHint::kAfter) {
    // If it's after the split, it should probably be after all tabs in the
    // split.
    index += node.children().size();
  }
  return BrowserRootView::DropIndex{
      .index = index,
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex};
}

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForPinned() const {
  return BrowserRootView::DropIndex{
      .index = tab_strip_model_->IndexOfFirstNonPinnedTab(),
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex};
}

std::optional<BrowserRootView::DropIndex>
VerticalTabLinkDropHandler::GetDropIndexForUnpinned() const {
  return BrowserRootView::DropIndex{
      .index = tab_strip_model_->count(),
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex};
}
