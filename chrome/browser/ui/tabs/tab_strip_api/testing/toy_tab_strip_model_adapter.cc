// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api::testing {

ToyTabStripModelAdapter::ToyTabStripModelAdapter(ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

void ToyTabStripModelAdapter::AddModelObserver(
    TabStripModelObserver* observer) {}
void ToyTabStripModelAdapter::RemoveModelObserver(
    TabStripModelObserver* observer) {}
void ToyTabStripModelAdapter::AddCollectionObserver(
    tabs::TabCollectionObserver* collection_observer) {}

void ToyTabStripModelAdapter::RemoveCollectionObserver(
    tabs::TabCollectionObserver* collection_observer) {}

std::vector<tabs::TabHandle> ToyTabStripModelAdapter::GetTabs() const {
  return tab_strip_->GetTabs();
}

TabRendererData ToyTabStripModelAdapter::GetTabRendererData(int index) const {
  return TabRendererData();
}

tabs_api::converters::TabStates ToyTabStripModelAdapter::GetTabStates(
    tabs::TabHandle handle) const {
  return {
      .is_active = tab_strip_->GetToyTabFor(handle).active,
      .is_selected = tab_strip_->GetToyTabFor(handle).selected,
  };
}

const ui::ColorProvider& ToyTabStripModelAdapter::GetColorProvider() const {
  return color_provider_;
}

void ToyTabStripModelAdapter::CloseTab(size_t idx) {
  tab_strip_->CloseTab(idx);
}

std::optional<int> ToyTabStripModelAdapter::GetIndexForHandle(
    tabs::TabHandle tab_handle) const {
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

mojom::ContainerPtr ToyTabStripModelAdapter::GetTabStripTopology(
    tabs::TabCollection::Handle root) const {
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
    const tabs::TabCollection::Handle& collection_handle) const {
  return tab_strip_->GetGroupIdFor(collection_handle);
}

void ToyTabStripModelAdapter::UpdateTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  tab_strip_->UpdateGroupVisuals(group, visual_data);
}

void ToyTabStripModelAdapter::SetTabSelection(
    const std::vector<tabs::TabHandle>& handles_to_select,
    tabs::TabHandle to_activate) {
  std::set<tabs::TabHandle> selection(handles_to_select.begin(),
                                      handles_to_select.end());
  tab_strip_->SetTabSelection(selection);
  tab_strip_->SetActiveTab(to_activate);
}

std::optional<tab_groups::TabGroupId>
ToyTabStripModelAdapter::GetTabGroupForTab(int index) const {
  // TODO(crbug.com/412709271): Integrate with the toy tabstrip
  NOTIMPLEMENTED();
  return std::nullopt;
}

tabs::TabCollectionHandle
ToyTabStripModelAdapter::GetCollectionHandleForTabGroupId(
    tab_groups::TabGroupId group_id) const {
  // TODO(crbug.com/412709271): Integrate with the toy tabstrip
  NOTIMPLEMENTED();
  return tabs::TabCollectionHandle::Null();
}

tabs_api::Position ToyTabStripModelAdapter::GetPositionForAbsoluteIndex(
    int absolute_index) const {
  NOTIMPLEMENTED();
  return tabs_api::Position();
}

InsertionParams ToyTabStripModelAdapter::CalculateInsertionParams(
    const std::optional<tabs_api::Position>& pos) const {
  NOTIMPLEMENTED();
  return tabs_api::InsertionParams();
}

const tabs::TabCollection* ToyTabStripModelAdapter::GetRoot() const {
  return tab_strip_->GetRoot().Get();
}

}  // namespace tabs_api::testing
