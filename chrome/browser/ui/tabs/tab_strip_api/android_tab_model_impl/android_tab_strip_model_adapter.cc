// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"

#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"

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

void AndroidTabStripModelAdapter::MoveTab(tabs::TabHandle handle,
                                          const Position& position) {
  NOTREACHED() << "not implemented";
}

void AndroidTabStripModelAdapter::MoveCollection(const NodeId& id,
                                                 const Position& position) {
  NOTREACHED() << "not implemented";
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
  NOTREACHED() << "not implemented";
}

void AndroidTabStripModelAdapter::SetTabSelection(
    const std::vector<tabs::TabHandle>& handles_to_select,
    tabs::TabHandle to_activate) {
  NOTREACHED() << "not implemented";
}

std::optional<tab_groups::TabGroupId>
AndroidTabStripModelAdapter::GetTabGroupForTab(int index) const {
  NOTREACHED() << "not implemented";
}

tabs::TabCollectionHandle
AndroidTabStripModelAdapter::GetCollectionHandleForTabGroupId(
    tab_groups::TabGroupId group_id) const {
  NOTREACHED() << "not implemented";
}

tabs::TabCollectionHandle
AndroidTabStripModelAdapter::GetCollectionHandleForSplitTabId(
    split_tabs::SplitTabId split_id) const {
  NOTREACHED() << "not implemented";
}

tabs_api::Position AndroidTabStripModelAdapter::GetPositionForAbsoluteIndex(
    int absolute_index) const {
  NOTREACHED() << "not implemented";
}

tabs_api::Path AndroidTabStripModelAdapter::GetPathForCollection(
    tabs::TabCollectionHandle collection_handle) const {
  NOTREACHED() << "not implemented";
}

InsertionParams AndroidTabStripModelAdapter::CalculateInsertionParams(
    const std::optional<tabs_api::Position>& pos) const {
  // TODO(crbug.com/494284032): hardcoded to always insert at the end for now.
  return {.index = model_->GetTabCount(),
          .group_id = std::nullopt,
          .pinned = false};
}

void AndroidTabStripModelAdapter::ReplaceTabInSplit(
    tabs::TabHandle tab_to_replace,
    int tab_to_insert_index) {
  NOTREACHED() << "not implemented";
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
