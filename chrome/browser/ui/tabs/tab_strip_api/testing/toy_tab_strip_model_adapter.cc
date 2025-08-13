// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"

namespace tabs_api::testing {

ToyTabStripModelAdapter::ToyTabStripModelAdapter(ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

void ToyTabStripModelAdapter::AddObserver(TabStripModelObserver*) {}
void ToyTabStripModelAdapter::RemoveObserver(TabStripModelObserver*) {}

std::vector<tabs::TabHandle> ToyTabStripModelAdapter::GetTabs() const {
  return tab_strip_->GetTabs();
}

TabRendererData ToyTabStripModelAdapter::GetTabRendererData(int index) const {
  return TabRendererData();
}

const ui::ColorProvider& ToyTabStripModelAdapter::GetColorProvider() const {
  return color_provider_;
}

void ToyTabStripModelAdapter::CloseTab(size_t idx) {
  tab_strip_->CloseTab(idx);
}

std::optional<int> ToyTabStripModelAdapter::GetIndexForHandle(
    tabs::TabHandle tab_handle) {
  return tab_strip_->GetIndexForHandle(tab_handle);
}

void ToyTabStripModelAdapter::ActivateTab(size_t idx) {
  const auto tab = tab_strip_->GetTabs().at(idx);
  tab_strip_->ActivateTab(tab);
}

void ToyTabStripModelAdapter::MoveTab(tabs::TabHandle handle,
                                      const Position& position) {
  tab_strip_->MoveTab(handle, position.index());
}

void ToyTabStripModelAdapter::MoveCollection(const NodeId& id,
                                             const Position& position) {
  // TODO(crbug.com/412709271): Integrate with the toy tabstrip to move a
  // collection.
  NOTIMPLEMENTED();
  return;
}

mojom::ContainerPtr ToyTabStripModelAdapter::GetTabStripTopology() {
  auto mojo_tab_strip = tabs_api::mojom::TabStrip::New();
  mojo_tab_strip->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kCollection, "0");

  auto result = tabs_api::mojom::Container::New();
  result->data = tabs_api::mojom::Data::NewTabStrip(std::move(mojo_tab_strip));

  std::vector<tabs::TabHandle> tabs = tab_strip_->GetTabs();
  for (auto& handle : tabs) {
    auto tab = tabs_api::mojom::Tab::New();
    tab->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
    auto child_container = tabs_api::mojom::Container::New();
    child_container->data = tabs_api::mojom::Data::NewTab(std::move(tab));
    result->children.push_back(std::move(child_container));
  }
  return result;
}

std::optional<const tab_groups::TabGroupId>
ToyTabStripModelAdapter::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) {
  return tab_strip_->GetGroupIdFor(collection_handle);
}

void ToyTabStripModelAdapter::UpdateTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  tab_strip_->UpdateGroupVisuals(group, visual_data);
}

}  // namespace tabs_api::testing
