// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"

#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"

namespace tabs_api {

AndroidTabStripModelAdapter::AndroidTabStripModelAdapter(TabModel* model)
    : model_(CHECK_DEREF(model)),
      root_(model_->GetTabStripCollection(GetPassKey())) {
  CHECK(root_ != nullptr) << "root tab strip handle cannot be null";
}

AndroidTabStripModelAdapter::~AndroidTabStripModelAdapter() = default;

std::vector<tabs::TabHandle> AndroidTabStripModelAdapter::GetTabs() const {
  std::vector<tabs::TabHandle> handles;
  for (auto* tab_interface : model_->GetAllTabs()) {
    handles.push_back(tab_interface->GetHandle());
  }
  return handles;
}

types::TabStates AndroidTabStripModelAdapter::GetTabStates(
    tabs::TabHandle handle) const {
  CHECK(handle.Get());
  return types::TabStates{
      .is_active = handle.Get()->IsActivated(),
      .is_selected = handle.Get()->IsSelected(),
  };
}

const ui::ColorProvider& AndroidTabStripModelAdapter::GetColorProvider() const {
  auto& content = CHECK_DEREF(model_->GetActiveWebContents());
  return content.GetColorProvider();
}

void AndroidTabStripModelAdapter::CloseTab(size_t tab_index) {
  auto tabs = GetTabs();
  CHECK(tab_index < tabs.size());
  model_->CloseTab(tabs.at(tab_index));
}

void AndroidTabStripModelAdapter::CloseTabGroup(
    const tab_groups::TabGroupId& group_id) {
  auto tabs_to_close = model_->GetTabGroupTabIndices(group_id).ToIntVector();

  std::reverse(tabs_to_close.begin(), tabs_to_close.end());
  auto& reversed_to_avoid_iterator_invalidation = tabs_to_close;
  for (const auto& tab_idx : reversed_to_avoid_iterator_invalidation) {
    CloseTab(tab_idx);
  }

  CHECK(!model_->ContainsTabGroup(group_id))
      << "expected group to be deleted after all tabs have clsoed, but it is "
         "not";
}

std::optional<int> AndroidTabStripModelAdapter::GetIndexForHandle(
    tabs::TabHandle tab_handle) const {
  for (int i = 0; i < model_->GetTabCount(); ++i) {
    if (model_->GetTab(i)->GetHandle() == tab_handle) {
      return i;
    }
  }
  return std::nullopt;
}

void AndroidTabStripModelAdapter::ActivateTab(size_t index) {
  auto handles = GetTabs();
  CHECK(index < handles.size());
  model_->ActivateTab(handles.at(index));
}

base::expected<void, mojo_base::mojom::ErrorPtr>
AndroidTabStripModelAdapter::MoveTab(tabs::TabHandle handle,
                                     const Position& position) {
  auto maybe_index = GetIndexForHandle(handle);
  CHECK(maybe_index.has_value());

  NodeId parent_id;
  if (position.path().components().empty()) {
    parent_id = NodeId::FromTabCollectionHandle(
        root_->unpinned_collection()->GetHandle());
  } else {
    parent_id = position.path().components().back();
  }

  std::optional<tabs::TabCollectionHandle> collection_handle =
      parent_id.ToTabCollectionHandle();
  CHECK(collection_handle.has_value());
  const tabs::TabCollection* collection = collection_handle.value().Get();

  const bool to_pinned =
      (collection->type() == tabs::TabCollection::Type::PINNED);
  if (to_pinned != handle.Get()->IsPinned()) {
    if (to_pinned) {
      model_->PinTab(handle);
    } else {
      model_->UnpinTab(handle);
    }
  }

  int to_position = 0;
  std::optional<tab_groups::TabGroupId> to_group;
  switch (collection->type()) {
    case tabs::TabCollection::Type::PINNED:
      to_position = position.index();
      break;

    case tabs::TabCollection::Type::GROUP: {
      to_group = FindGroupIdFor(collection_handle.value());
      CHECK(to_group.has_value());
      to_position = model_->GetTabGroupTabIndices(to_group.value()).start() +
                    position.index();
      break;
    }
    case tabs::TabCollection::Type::UNPINNED:
      to_position = root_->IndexOfFirstNonPinnedTab() + position.index();
      break;
    case tabs::TabCollection::Type::TABSTRIP:
    case tabs::TabCollection::Type::SPLIT:
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "unsupported move target"));
  }

  model_->MoveTab(handle, to_position);

  if (to_group.has_value()) {
    model_->AddTabsToGroup(to_group, {handle});
  } else if (handle.Get()->GetGroup().has_value()) {
    model_->Ungroup({handle});
  }

  return base::ok();
}

base::expected<void, mojo_base::mojom::ErrorPtr>
AndroidTabStripModelAdapter::MoveCollection(const NodeId& id,
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
      CHECK(group_id.has_value());

      NodeId parent_id;
      if (position.path().components().empty()) {
        parent_id = NodeId::FromTabCollectionHandle(
            root_->unpinned_collection()->GetHandle());
      } else {
        parent_id = position.path().components().back();
      }

      std::optional<tabs::TabCollectionHandle> parent_handle =
          parent_id.ToTabCollectionHandle();
      CHECK(parent_handle.has_value());
      const tabs::TabCollection* parent_collection =
          parent_handle.value().Get();

      int to_position = 0;
      if (parent_collection->type() == tabs::TabCollection::Type::UNPINNED) {
        to_position = root_->IndexOfFirstNonPinnedTab() + position.index();
      } else if (parent_collection->type() ==
                 tabs::TabCollection::Type::GROUP) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "nested tab groups are not supported on Android"));
      } else {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Unsupported parent collection type for MoveCollection"));
      }

      model_->MoveGroupTo(group_id.value(), to_position);
      break;
    }
    default:
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument,
          "Unsupported collection type for MoveCollection"));
  }

  return base::ok();
}

mojom::ContainerPtr AndroidTabStripModelAdapter::GetTabStripTopology(
    tabs::TabCollection::Handle root) const {
  auto window_container = mojom::Container::New();
  auto window_data = mojom::Window::New();

  window_data->id = NodeId::FromWindowId(GetWindowId());
  window_container->data = mojom::Data::NewWindow(std::move(window_data));

  auto tab_strip = tabs_api::mojom::Container::New();
  auto tab_strip_data = mojom::TabStrip::New();
  tab_strip_data->id = NodeId::FromTabCollectionHandle(root);
  tab_strip->data = mojom::Data::NewTabStrip(std::move(tab_strip_data));

  for (auto* tab_interface : model_->GetAllTabs()) {
    auto tab = tabs_api::mojom::Container::New();
    auto tab_data = mojom::Tab::New();
    tab_data->id = tabs_api::NodeId::FromTabHandle(tab_interface->GetHandle());
    tab->data = mojom::Data::NewTab(std::move(tab_data));
    tab_strip->children.emplace_back(std::move(tab));
  }

  window_container->children.emplace_back(std::move(tab_strip));
  return window_container;
}

std::optional<const tab_groups::TabGroupId>
AndroidTabStripModelAdapter::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) const {
  auto* tab_strip = model_->GetTabStripCollection(GetPassKey());

  for (auto& group_id : tab_strip->GetAllTabGroupIds()) {
    auto* maybe_collection = tab_strip->GetTabGroupCollection(group_id);
    if (maybe_collection != nullptr) {
      if (maybe_collection->GetHandle() == collection_handle) {
        return group_id;
      }
    }
  }

  return std::nullopt;
}

void AndroidTabStripModelAdapter::UpdateTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  model_->SetTabGroupVisualData(group, visual_data);
}

void AndroidTabStripModelAdapter::SetTabSelection(
    const std::vector<tabs::TabHandle>& handles_to_select,
    tabs::TabHandle to_activate) {
  std::set<tabs::TabHandle> handles;
  for (const auto& handle : handles_to_select) {
    handles.insert(handle);
  }
  model_->HighlightTabs(to_activate, handles);
}

std::optional<tab_groups::TabGroupId>
AndroidTabStripModelAdapter::GetTabGroupForTab(int index) const {
  return model_->GetTabAt(index)->GetGroup();
}

tabs::TabCollectionHandle
AndroidTabStripModelAdapter::GetCollectionHandleForTabGroupId(
    tab_groups::TabGroupId group_id) const {
  auto* collection = root_->GetTabGroupCollection(group_id);
  CHECK(collection);
  return collection->GetHandle();
}

tabs::TabCollectionHandle
AndroidTabStripModelAdapter::GetCollectionHandleForSplitTabId(
    split_tabs::SplitTabId split_id) const {
  auto* collection = root_->GetSplitTabCollection(split_id);
  CHECK(collection);
  return collection->GetHandle();
}

tabs_api::Position AndroidTabStripModelAdapter::GetPositionForAbsoluteIndex(
    int absolute_index) const {
  auto* tab = model_->GetTabAt(absolute_index);
  CHECK(tab);
  auto* collection = tab->GetParentCollection();

  if (collection) {
    auto relative_index = collection->GetIndexOfTab(tab);
    if (relative_index.has_value()) {
      return tabs_api::Position(relative_index.value(),
                                GetPathForCollection(collection->GetHandle()));
    }
  }

  // Fallback to absolute index and tab strip root if collection is not ready.
  return tabs_api::Position(absolute_index,
                            GetPathForCollection(root_->GetHandle()));
}

tabs_api::Path AndroidTabStripModelAdapter::GetPathForCollection(
    tabs::TabCollectionHandle collection_handle) const {
  std::vector<tabs_api::NodeId> components;
  components.push_back(NodeId::FromWindowId(GetWindowId()));

  // Traversal is bottom-up (child -> root), but the path requires a top-down
  // representation (root -> child).
  std::vector<tabs_api::NodeId> collection_components;
  const tabs::TabCollection* curr = collection_handle.Get();
  while (curr) {
    collection_components.push_back(
        NodeId::FromTabCollectionHandle(curr->GetHandle()));
    curr = curr->GetParentCollection();
  }
  std::reverse(collection_components.begin(), collection_components.end());
  components.insert(components.end(),
                    std::make_move_iterator(collection_components.begin()),
                    std::make_move_iterator(collection_components.end()));

  return tabs_api::Path(std::move(components));
}

InsertionParams AndroidTabStripModelAdapter::CalculateInsertionParams(
    const std::optional<tabs_api::Position>& pos) const {
  if (!pos.has_value()) {
    return {.index = model_->GetTabCount(),
            .group_id = std::nullopt,
            .pinned = false};
  }

  const auto& position = pos.value();
  NodeId parent_id;
  if (position.path().components().empty()) {
    parent_id = NodeId::FromTabCollectionHandle(
        root_->unpinned_collection()->GetHandle());
  } else {
    parent_id = position.path().components().back();
  }

  std::optional<tabs::TabCollectionHandle> collection_handle =
      parent_id.ToTabCollectionHandle();
  CHECK(collection_handle.has_value());
  const tabs::TabCollection* collection = collection_handle.value().Get();

  // Assuming that pinned tabs are always at the start of tab lists.
  int index = 0;
  std::optional<tab_groups::TabGroupId> group_id;

  switch (collection->type()) {
    case tabs::TabCollection::Type::PINNED:
      index = position.index();
      break;
    case tabs::TabCollection::Type::UNPINNED:
      index = root_->IndexOfFirstNonPinnedTab() + position.index();
      break;

    case tabs::TabCollection::Type::GROUP:
      group_id = FindGroupIdFor(collection_handle.value());
      CHECK(group_id.has_value());
      index = model_->GetTabGroupTabIndices(group_id.value()).start() +
              position.index();
      break;

    case tabs::TabCollection::Type::TABSTRIP:
    case tabs::TabCollection::Type::SPLIT:
      NOTREACHED() << "Unsupported collection type for insertion";
  }

  bool pinned = (collection->type() == tabs::TabCollection::Type::PINNED);
  return {.index = index, .group_id = group_id, .pinned = pinned};
}

base::expected<void, mojo_base::mojom::ErrorPtr>
AndroidTabStripModelAdapter::ReplaceTabInSplit(tabs::TabHandle tab_to_replace,
                                               int tab_to_insert_index) {
  return base::unexpected(
      mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                   "split tabs are not supported on Android"));
}

const tabs::TabCollection* AndroidTabStripModelAdapter::GetRoot() const {
  return root_;
}

std::string AndroidTabStripModelAdapter::GetWindowId() const {
  return base::NumberToString(model_->GetSessionId().id());
}

base::PassKey<AndroidTabStripModelAdapter>
AndroidTabStripModelAdapter::GetPassKey() {
  return base::PassKey<AndroidTabStripModelAdapter>();
}

}  // namespace tabs_api
