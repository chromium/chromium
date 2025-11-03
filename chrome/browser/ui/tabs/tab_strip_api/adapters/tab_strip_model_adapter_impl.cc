// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "content/public/browser/web_contents.h"

namespace tabs_api {

void TabStripModelAdapterImpl::AddModelObserver(
    TabStripModelObserver* tab_strip_model_observer) {
  tab_strip_model_->AddObserver(tab_strip_model_observer);
}

void TabStripModelAdapterImpl::RemoveModelObserver(
    TabStripModelObserver* observer) {
  tab_strip_model_->RemoveObserver(observer);
}

void TabStripModelAdapterImpl::AddCollectionObserver(
    tabs::TabCollectionObserver* collection_observer) {
  tab_strip_model_->Root(base::PassKey<TabStripModelAdapterImpl>())
      ->AddObserver(collection_observer);
}

void TabStripModelAdapterImpl::RemoveCollectionObserver(
    tabs::TabCollectionObserver* collection_observer) {
  tab_strip_model_->Root(base::PassKey<TabStripModelAdapterImpl>())
      ->RemoveObserver(collection_observer);
}

std::vector<tabs::TabHandle> TabStripModelAdapterImpl::GetTabs() const {
  std::vector<tabs::TabHandle> tabs;
  for (auto* tab : *tab_strip_model_) {
    tabs.push_back(tab->GetHandle());
  }
  return tabs;
}

TabRendererData TabStripModelAdapterImpl::GetTabRendererData(int index) const {
  return TabRendererData::FromTabInModel(tab_strip_model_, index);
}

converters::TabStates TabStripModelAdapterImpl::GetTabStates(
    tabs::TabHandle handle) const {
  CHECK(handle.Get() != nullptr);
  return {
      .is_active = handle.Get()->IsActivated(),
      .is_selected = handle.Get()->IsSelected(),
  };
}

const ui::ColorProvider& TabStripModelAdapterImpl::GetColorProvider() const {
  content::WebContents* active_contents =
      tab_strip_model_->GetActiveWebContents();
  CHECK(active_contents);
  return active_contents->GetColorProvider();
}

void TabStripModelAdapterImpl::CloseTab(size_t tab_index) {
  tab_strip_model_->CloseWebContentsAt(tab_index, TabCloseTypes::CLOSE_NONE);
}

std::optional<int> TabStripModelAdapterImpl::GetIndexForHandle(
    tabs::TabHandle tab_handle) const {
  auto idx = tab_strip_model_->GetIndexOfTab(tab_handle.Get());
  return idx != TabStripModel::kNoTab ? std::make_optional(idx) : std::nullopt;
}

void TabStripModelAdapterImpl::ActivateTab(size_t index) {
  tab_strip_model_->ActivateTabAt(index);
}

void TabStripModelAdapterImpl::MoveTab(tabs::TabHandle tab,
                                       const Position& position) {
  auto maybe_index = GetIndexForHandle(tab);
  CHECK(maybe_index.has_value());
  auto index = maybe_index.value();
  // If the position has no parent, move within the unpinned collection.
  if (!position.parent_id().has_value()) {
    const int to_position =
        tab_strip_model_->IndexOfFirstNonPinnedTab() + position.index();
    tab_strip_model_->MoveWebContentsAt(index, to_position,
                                        /*select_after_move=*/false,
                                        /*group=*/std::nullopt);
    return;
  }

  std::optional<tabs::TabCollectionHandle> collection_handle =
      position.parent_id().value().ToTabCollectionHandle();
  CHECK(collection_handle.has_value());
  const tabs::TabCollection* collection = collection_handle.value().Get();
  const bool to_pinned =
      (collection->type() == tabs::TabCollection::Type::PINNED);
  if (to_pinned != tab_strip_model_->IsTabPinned(index)) {
    // Modify the start position if tab has been moved from pinned to
    // unpinned or vice versa.
    index = tab_strip_model_->SetTabPinned(index, to_pinned);
  }

  // Calculate the absolute index based on the collection type.
  int to_position = 0;
  std::optional<tab_groups::TabGroupId> to_group;
  switch (collection->type()) {
    case tabs::TabCollection::Type::PINNED:
      // For the pinned collection, the index is absolute from the start.
      to_position = position.index();
      break;

    case tabs::TabCollection::Type::GROUP: {
      to_group = FindGroupIdFor(*collection_handle);
      CHECK(to_group.has_value());
      const TabGroup* group =
          tab_strip_model_->group_model()->GetTabGroup(*to_group);
      CHECK(group);
      to_position = group->ListTabs().start() + position.index();
      break;
    }
    case tabs::TabCollection::Type::UNPINNED:
      to_position =
          tab_strip_model_->IndexOfFirstNonPinnedTab() + position.index();
      break;
    // TODO(crbug.com/412709271) Callers can not move a Tab within TabStrip and
    // SplitTab collections. This should return an error to the client.
    case tabs::TabCollection::Type::TABSTRIP:
    case tabs::TabCollection::Type::SPLIT:
      NOTIMPLEMENTED();
      return;
  }

  tab_strip_model_->MoveWebContentsAt(index, to_position,
                                      /*select_after_move=*/false, to_group);
}

void TabStripModelAdapterImpl::MoveCollection(const NodeId& id,
                                              const Position& position) {
  std::optional<tabs::TabCollectionHandle> collection_handle =
      id.ToTabCollectionHandle();
  CHECK(collection_handle.has_value());

  const tabs::TabCollection* collection = collection_handle.value().Get();
  CHECK(collection);

  switch (collection->type()) {
    case tabs::TabCollection::Type::GROUP: {
      std::optional<const tab_groups::TabGroupId> group_id =
          FindGroupIdFor(collection_handle.value());
      // TODO(crbug.com/409086859): Invalid group id is a user supplied data and
      // should result in API failure.
      CHECK(group_id.has_value());
      const int to_position =
          tab_strip_model_->IndexOfFirstNonPinnedTab() + position.index();
      tab_strip_model_->MoveGroupTo(group_id.value(), to_position);
      break;
    }
    case tabs::TabCollection::Type::SPLIT: {
      const tabs::SplitTabCollection* split_collection =
          static_cast<const tabs::SplitTabCollection*>(collection);
      const split_tabs::SplitTabId split_id = split_collection->GetSplitTabId();
      const int to_position =
          tab_strip_model_->IndexOfFirstNonPinnedTab() + position.index();
      // TODO(crbug.com/412709271): Currently only moves within the unpinned
      // collection.
      tab_strip_model_->MoveSplitTo(split_id, to_position, false /* pinned */,
                                    std::nullopt);
      break;
    }
    case tabs::TabCollection::Type::PINNED:
    case tabs::TabCollection::Type::UNPINNED:
    case tabs::TabCollection::Type::TABSTRIP:
      NOTIMPLEMENTED();
      return;
  }
}

tabs_api::mojom::ContainerPtr TabStripModelAdapterImpl::GetTabStripTopology(
    tabs::TabCollection::Handle root) const {
  return MojoTreeBuilder(tab_strip_model_).Build(root);
}

std::optional<const tab_groups::TabGroupId>
TabStripModelAdapterImpl::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) const {
  return tab_strip_model_->FindGroupIdFor(
      collection_handle, base::PassKey<TabStripModelAdapterImpl>());
}

void TabStripModelAdapterImpl::UpdateTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  tab_strip_model_->ChangeTabGroupVisuals(group, visual_data,
                                          false /*is_customized*/);
}

void TabStripModelAdapterImpl::SetTabSelection(
    const std::vector<tabs::TabHandle>& handles_to_select,
    tabs::TabHandle to_activate) {
  // TODO: we should have input validation to ensure that all handles can be
  // exchanged to indices.
  auto active_index = GetIndexForHandle(to_activate);
  CHECK(active_index.has_value());

  std::vector<size_t> tab_indices;
  for (auto& handle : handles_to_select) {
    auto index = GetIndexForHandle(handle);
    CHECK(index.has_value());
    tab_indices.push_back(index.value());
  }

  ui::ListSelectionModel selection;
  for (auto& tab_index : tab_indices) {
    selection.AddIndexToSelection(tab_index);
  }
  selection.set_active(active_index.value());

  tab_strip_model_->SetSelectionFromModel(selection);
}

std::optional<tab_groups::TabGroupId>
TabStripModelAdapterImpl::GetTabGroupForTab(int index) const {
  return tab_strip_model_->GetTabGroupForTab(index);
}

tabs::TabCollectionHandle
TabStripModelAdapterImpl::GetCollectionHandleForTabGroupId(
    tab_groups::TabGroupId group_id) const {
  const TabGroup* tab_group =
      tab_strip_model_->group_model()->GetTabGroup(group_id);
  return tab_group->GetCollectionHandle();
}

tabs_api::Position TabStripModelAdapterImpl::GetPositionForAbsoluteIndex(
    int absolute_index) const {
  const auto tab_group_id = GetTabGroupForTab(absolute_index);
  int relative_index =
      absolute_index - tab_strip_model_->IndexOfFirstNonPinnedTab();
  std::optional<tabs_api::NodeId> parent_id =
      NodeId::FromTabCollectionHandle(GetUnpinnedTabsCollectionHandle());

  if (absolute_index < tab_strip_model_->IndexOfFirstNonPinnedTab()) {
    relative_index = absolute_index;
    parent_id =
        NodeId::FromTabCollectionHandle(GetPinnedTabsCollectionHandle());
  } else if (tab_group_id.has_value()) {
    const TabGroup* tab_group =
        tab_strip_model_->group_model()->GetTabGroup(tab_group_id.value());
    relative_index = absolute_index - tab_group->ListTabs().start();
    parent_id = NodeId::FromTabCollectionHandle(
        GetCollectionHandleForTabGroupId(tab_group_id.value()));
  }

  return tabs_api::Position(relative_index, parent_id);
}

InsertionParams TabStripModelAdapterImpl::CalculateInsertionParams(
    const std::optional<tabs_api::Position>& pos) const {
  tabs_api::InsertionParams params;
  if (pos.has_value()) {
    params.index = pos->index();
    const std::optional<tabs_api::NodeId>& parent_id = pos->parent_id();
    if (parent_id.has_value()) {
      if (parent_id.value() ==
          NodeId::FromTabCollectionHandle(GetPinnedTabsCollectionHandle())) {
        params.pinned = true;
      } else {
        std::optional<tabs::TabCollectionHandle> collection_handle =
            parent_id.value().ToTabCollectionHandle();
        if (collection_handle.has_value()) {
          params.group_id = FindGroupIdFor(collection_handle.value());
        }
      }
    }
  }

  return params;
}

const tabs::TabCollection* TabStripModelAdapterImpl::GetRoot() const {
  return tab_strip_model_->Root(base::PassKey<TabStripModelAdapterImpl>());
}

tabs::TabCollectionHandle
TabStripModelAdapterImpl::GetPinnedTabsCollectionHandle() const {
  return tab_strip_model_->GetPinnedTabsCollectionHandle(
      base::PassKey<TabStripModelAdapterImpl>());
}

tabs::TabCollectionHandle
TabStripModelAdapterImpl::GetUnpinnedTabsCollectionHandle() const {
  return tab_strip_model_->GetUnpinnedTabsCollectionHandle(
      base::PassKey<TabStripModelAdapterImpl>());
}

}  // namespace tabs_api
