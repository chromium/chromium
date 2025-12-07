// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"

namespace tabs_api::events {

mojom::OnTabsCreatedEventPtr ToEvent(
    const tabs::TabHandle& handle,
    const tabs::TabCollection::Position& position,
    const tabs_api::TabStripModelAdapter* adapter) {
  auto event = mojom::OnTabsCreatedEvent::New();

    auto tab_created = tabs_api::mojom::TabCreatedContainer::New();
    tab_created->position = tabs_api::Position(
        position.index,
        NodeId::FromTabCollectionHandle(position.parent_handle));
    auto renderer_data =
        adapter->GetTabRendererData(adapter->GetIndexForHandle(handle).value());
    const ui::ColorProvider& provider = adapter->GetColorProvider();
    auto mojo_tab = tabs_api::converters::BuildMojoTab(
        handle, renderer_data, provider, adapter->GetTabStates(handle));

    tab_created->tab = std::move(mojo_tab);
    event->tabs.emplace_back(std::move(tab_created));

    return event;
}

mojom::OnCollectionCreatedEventPtr ToEvent(
    const tabs::TabCollectionHandle& handle,
    const tabs::TabCollection::Position& position,
    const tabs_api::TabStripModelAdapter* adapter,
    bool insert_from_detached) {
  auto event = mojom::OnCollectionCreatedEvent::New();
  event->position = tabs_api::Position(
      position.index, NodeId::FromTabCollectionHandle(position.parent_handle));

  if (!insert_from_detached) {
    event->collection = tabs_api::mojom::Container::New();
    event->collection->data =
        tabs_api::converters::BuildMojoTabCollectionData(handle);
  } else {
    event->collection = adapter->GetTabStripTopology(handle);
  }
  return event;
}

mojom::OnTabsClosedEventPtr ToEvent(
    const tabs::TabCollectionNodes& removed_handles) {
  auto event = mojom::OnTabsClosedEvent::New();

  for (const auto& handle_variant : removed_handles) {
    if (auto* tab_handle = std::get_if<tabs::TabHandle>(&handle_variant)) {
      event->tabs.emplace_back(NodeId::Type::kContent,
                               base::NumberToString(tab_handle->raw_value()));
    } else if (auto* collection_handle =
                   std::get_if<tabs::TabCollectionHandle>(&handle_variant)) {
      event->tabs.emplace_back(
          NodeId::Type::kCollection,
          base::NumberToString(collection_handle->raw_value()));
    }
  }

  return event;
}

mojom::OnNodeMovedEventPtr ToEvent(
    const tabs::TabCollection::Position& to_position,
    const tabs::TabCollection::Position& from_position,
    const tabs::TabCollection::NodeHandle node_handle) {
  enum NodeId::Type node_type;
  std::string handle;
  if (auto* tab_handle = std::get_if<tabs::TabHandle>(&node_handle)) {
    node_type = NodeId::Type::kContent;
    handle = base::NumberToString(tab_handle->raw_value());
  } else {
    auto* collection_handle =
        std::get_if<tabs::TabCollectionHandle>(&node_handle);
    node_type = NodeId::Type::kCollection;
    handle = base::NumberToString(collection_handle->raw_value());
  }
  NodeId id(node_type, handle);

  auto from = tabs_api::Position(
      from_position.index,
      NodeId::FromTabCollectionHandle(from_position.parent_handle));
  auto to = tabs_api::Position(
      to_position.index,
      NodeId::FromTabCollectionHandle(to_position.parent_handle));

  auto event = mojom::OnNodeMovedEvent::New();
  event->id = id;
  event->from = std::move(from);
  event->to = std::move(to);

  return event;
}

mojom::OnDataChangedEventPtr ToEvent(
    const tabs_api::TabStripModelAdapter* adapter,
    size_t index,
    TabChangeType change_type) {
  auto event = mojom::OnDataChangedEvent::New();
  auto tabs = adapter->GetTabs();
  if (index < tabs.size()) {
    auto& handle = tabs.at(index);
    auto renderer_data = adapter->GetTabRendererData(index);
    const ui::ColorProvider& color_provider = adapter->GetColorProvider();
    auto mojo_tab = tabs_api::converters::BuildMojoTab(
        handle, renderer_data, color_provider, adapter->GetTabStates(handle));
    event->data = mojom::Data::NewTab(std::move(mojo_tab));
  }

  return event;
}

std::vector<Event> ToEvent(const TabStripSelectionChange& selection,
                           const tabs_api::TabStripModelAdapter* adapter) {
  std::set<tabs::TabHandle> affected_tabs;

  if (selection.active_tab_changed()) {
    if (selection.old_tab) {
      affected_tabs.insert(selection.old_tab->GetHandle());
    }
    if (selection.new_tab) {
      affected_tabs.insert(selection.new_tab->GetHandle());
    }
  }

  if (selection.selection_changed()) {
    auto old_selected = selection.old_model.selected_indices();
    auto new_selected = selection.new_model.selected_indices();
    auto selected_diff =
        base::STLSetDifference<std::set<size_t>>(old_selected, new_selected);

    auto tabs = adapter->GetTabs();
    // TODO(crbug.com/412738255): There is a bug here where a selected state
    // might not be correctly cleared due to index shift. This is very
    // difficult to solve at this point, so we should probably change the
    // selection change event to use handles instead of indices to fix this
    // issue.
    for (auto& diff_tab_idx : selected_diff) {
      if (diff_tab_idx >= tabs.size()) {
        continue;
      }
      affected_tabs.insert(tabs.at(diff_tab_idx));
    }
  }

  std::vector<Event> events;
  for (auto& affected_tab : affected_tabs) {
    if (!adapter->GetIndexForHandle(affected_tab).has_value()) {
      continue;
    }
    auto event = mojom::OnDataChangedEvent::New();
    auto renderer_data = adapter->GetTabRendererData(
        adapter->GetIndexForHandle(affected_tab).value());
    const ui::ColorProvider& color_provider = adapter->GetColorProvider();
    auto mojo_tab = tabs_api::converters::BuildMojoTab(
        affected_tab, renderer_data, color_provider,
        adapter->GetTabStates(affected_tab));
    event->data = mojom::Data::NewTab(std::move(mojo_tab));
    events.push_back(std::move(event));
  }
  return events;
}

mojom::OnDataChangedEventPtr ToEvent(const TabGroupChange& tab_group_change) {
  CHECK_EQ(tab_group_change.type, TabGroupChange::Type::kVisualsChanged);
  TabGroup* tab_group = tab_group_change.model->group_model()->GetTabGroup(
      tab_group_change.group);
  auto event = mojom::OnDataChangedEvent::New();
  event->data = tabs_api::converters::BuildMojoTabCollectionData(
      tab_group->GetCollectionHandle());
  return event;
}

}  // namespace tabs_api::events
