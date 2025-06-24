// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"

#include <utility>

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
                                      Position position) {
  tab_strip_->MoveTab(handle, position.index);
}

mojom::TabCollectionContainerPtr
ToyTabStripModelAdapter::GetTabStripTopology() {
  auto tab_collection = tabs_api::mojom::TabCollection::New();
  tab_collection->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kCollection, "0");
  tab_collection->collection_type =
      tabs_api::mojom::TabCollection::CollectionType::kTabStrip;

  auto result = tabs_api::mojom::TabCollectionContainer::New();
  result->collection = std::move(tab_collection);

  std::vector<tabs::TabHandle> tabs = tab_strip_->GetTabs();
  for (auto& handle : tabs) {
    auto tab = tabs_api::mojom::Tab::New();
    tab->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
    auto tab_container = tabs_api::mojom::TabContainer::New();
    tab_container->tab = std::move(tab);
    auto element =
        tabs_api::mojom::Container::NewTabContainer(std::move(tab_container));
    result->elements.push_back(std::move(element));
  }
  return result;
}

}  // namespace tabs_api::testing
