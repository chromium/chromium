// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"

#include "base/notreached.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs_api {

class FakedAndroidTabCollection : public tabs::TabCollection {
 public:
  FakedAndroidTabCollection()
      : TabCollection(tabs::TabCollection::Type::TABSTRIP, {}, true) {}
  ~FakedAndroidTabCollection() override = default;
};

AndroidTabStripModelAdapter::AndroidTabStripModelAdapter(TabModel* model)
    : model_(*model),
      fake_root_(std::make_unique<FakedAndroidTabCollection>()) {}

AndroidTabStripModelAdapter::~AndroidTabStripModelAdapter() = default;

std::vector<tabs::TabHandle> AndroidTabStripModelAdapter::GetTabs() const {
  NOTREACHED() << "not implemented";
}

types::TabStates AndroidTabStripModelAdapter::GetTabStates(
    tabs::TabHandle) const {
  NOTREACHED() << "not implemented";
}

const ui::ColorProvider& AndroidTabStripModelAdapter::GetColorProvider() const {
  NOTREACHED() << "not implemented";
}

void AndroidTabStripModelAdapter::CloseTab(size_t tab_index) {
  NOTREACHED() << "not implemented";
}

std::optional<int> AndroidTabStripModelAdapter::GetIndexForHandle(
    tabs::TabHandle tab_handle) const {
  NOTREACHED() << "not implemented";
}

void AndroidTabStripModelAdapter::ActivateTab(size_t index) {
  NOTREACHED() << "not implemented";
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
  // Simulate a single tab in a single tab strip in a single window.
  // TODO(crbug.com/494284032): Remove.
  auto window_container = mojom::Container::New();
  auto window_data = mojom::Window::New();

  window_data->id = NodeId::FromWindowId("-");
  window_container->data = mojom::Data::NewWindow(std::move(window_data));

  auto tab_strip = tabs_api::mojom::Container::New();
  auto tab_strip_data = mojom::TabStrip::New();
  tab_strip_data->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kCollection, "-");
  tab_strip->data = mojom::Data::NewTabStrip(std::move(tab_strip_data));

  auto tab = tabs_api::mojom::Container::New();
  auto tab_data = mojom::Tab::New();
  tab_data->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent, "1337");
  tab->data = mojom::Data::NewTab(std::move(tab_data));

  tab_strip->children.emplace_back(std::move(tab));
  window_container->children.emplace_back(std::move(tab_strip));

  return window_container;
}

std::optional<const tab_groups::TabGroupId>
AndroidTabStripModelAdapter::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) const {
  NOTREACHED() << "not implemented";
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
  NOTREACHED() << "not implemented";
}

void AndroidTabStripModelAdapter::ReplaceTabInSplit(
    tabs::TabHandle tab_to_replace,
    int tab_to_insert_index) {
  NOTREACHED() << "not implemented";
}

const tabs::TabCollection* AndroidTabStripModelAdapter::GetRoot() const {
  return fake_root_.get();
}

std::string AndroidTabStripModelAdapter::GetWindowId() const {
  NOTREACHED() << "not implemented";
}

}  // namespace tabs_api
