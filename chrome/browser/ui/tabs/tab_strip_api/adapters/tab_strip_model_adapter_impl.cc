// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api {

void TabStripModelAdapterImpl::AddObserver(TabStripModelObserver* observer) {
  tab_strip_model_->AddObserver(observer);
}

void TabStripModelAdapterImpl::RemoveObserver(TabStripModelObserver* observer) {
  tab_strip_model_->RemoveObserver(observer);
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

void TabStripModelAdapterImpl::CloseTab(size_t tab_index) {
  tab_strip_model_->CloseWebContentsAt(tab_index, TabCloseTypes::CLOSE_NONE);
}

std::optional<int> TabStripModelAdapterImpl::GetIndexForHandle(
    tabs::TabHandle tab_handle) {
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
    case tabs::TabCollection::Type::PINNED:
    case tabs::TabCollection::Type::UNPINNED:
    case tabs::TabCollection::Type::TABSTRIP:
    // TODO(412709271). Implement moving a SplitTab collection.
    case tabs::TabCollection::Type::SPLIT:
      NOTIMPLEMENTED();
      return;
  }
}

tabs_api::mojom::TabCollectionContainerPtr
TabStripModelAdapterImpl::GetTabStripTopology() {
  return MojoTreeBuilder(tab_strip_model_).Build();
}

std::optional<const tab_groups::TabGroupId>
TabStripModelAdapterImpl::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) {
  return tab_strip_model_->FindGroupIdFor(
      collection_handle, base::PassKey<TabStripModelAdapterImpl>());
}

void TabStripModelAdapterImpl::UpdateTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  tab_strip_model_->ChangeTabGroupVisuals(group, visual_data,
                                          false /*is_customized*/);
}

}  // namespace tabs_api
