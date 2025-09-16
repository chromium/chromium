// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/node_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"

namespace tabs_api::events {

mojom::OnTabsCreatedEventPtr ToEvent(
    const TabStripModelChange::Insert& insert,
    const tabs_api::TabStripModelAdapter* adapter) {
  auto event = mojom::OnTabsCreatedEvent::New();
  for (auto& content : insert.contents) {
    auto tab_created = tabs_api::mojom::TabCreatedContainer::New();
    auto pos = adapter->GetPositionForAbsoluteIndex(content.index);
    tab_created->position = std::move(pos);
    auto renderer_data = adapter->GetTabRendererData(content.index);
    const ui::ColorProvider& provider = adapter->GetColorProvider();
    auto mojo_tab = tabs_api::converters::BuildMojoTab(
        content.tab->GetHandle(), renderer_data, provider,
        adapter->GetTabStates(content.tab->GetHandle()));

    tab_created->tab = std::move(mojo_tab);
    event->tabs.emplace_back(std::move(tab_created));
  }
  return event;
}

mojom::OnTabsClosedEventPtr ToEvent(const TabStripModelChange::Remove& remove) {
  auto event = mojom::OnTabsClosedEvent::New();

  for (auto& content : remove.contents) {
    event->tabs.emplace_back(
        NodeId::Type::kContent,
        base::NumberToString(content.tab->GetHandle().raw_value()));
  }

  return event;
}

mojom::OnNodeMovedEventPtr ToEvent(
    const TabStripModelChange::Move& move,
    const tabs_api::TabStripModelAdapter* adapter) {
  NodeId id(NodeId::Type::kContent,
            base::NumberToString(move.tab->GetHandle().raw_value()));

  auto from = tabs_api::Position(move.from_index);

  std::optional<tabs_api::NodeId> to_parent_id;
  auto tab_group_id = adapter->GetTabGroupForTab(move.to_index);
  if (tab_group_id.has_value()) {
    to_parent_id = NodeId::FromTabCollectionHandle(
        adapter->GetCollectionHandleForTabGroupId(tab_group_id.value()));
  }
  auto to = tabs_api::Position(move.to_index, to_parent_id);

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

mojom::OnCollectionCreatedEventPtr FromTabGroupToDataCreatedEvent(
    const TabGroupChange& tab_group_change) {
  CHECK_EQ(tab_group_change.type, TabGroupChange::Type::kCreated);
  TabGroup* tab_group = tab_group_change.model->group_model()->GetTabGroup(
      tab_group_change.group);
  auto event = mojom::OnCollectionCreatedEvent::New();
  event->data = tabs_api::converters::BuildMojoTabCollectionData(
      tab_group->GetCollectionHandle());
  // TODO(crbug.com/412935315): Determine whether a position is necessary in a
  // OnCollectionCreated event. This will have no tabs unless it has been
  // inserted from another tabstrip.
  event->position = tabs_api::Position(0);
  // When TabGroupChange::kCreated is fired, the TabGroupTabCollection is
  // empty. Then, TabGroupedStateChanged() is fired, which adds tabs to the
  // group.
  return event;
}

mojom::OnNodeMovedEventPtr FromTabGroupedStateChangedToNodeMovedEvent(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group_id,
    std::optional<tab_groups::TabGroupId> new_group_id,
    tabs::TabInterface* tab,
    int index) {
  auto event = mojom::OnNodeMovedEvent::New();
  event->id = NodeId::FromTabHandle(tab->GetHandle());
  std::optional<tabs_api::NodeId> old_parent_id;
  if (old_group_id.has_value()) {
    TabGroup* old_group =
        tab_strip_model->group_model()->GetTabGroup(old_group_id.value());
    old_parent_id = tabs_api::NodeId(
        tabs_api::NodeId::Type::kCollection,
        base::NumberToString(old_group->GetCollectionHandle().raw_value()));
  }
  event->from = tabs_api::Position(0, old_parent_id);

  std::optional<tabs_api::NodeId> new_parent_id;
  if (new_group_id.has_value()) {
    TabGroup* new_group =
        tab_strip_model->group_model()->GetTabGroup(new_group_id.value());
    new_parent_id = tabs_api::NodeId(
        tabs_api::NodeId::Type::kCollection,
        base::NumberToString(new_group->GetCollectionHandle().raw_value()));
  }
  event->to = tabs_api::Position(index, new_parent_id);
  return event;
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

mojom::OnNodeMovedEventPtr ToTabGroupMovedEvent(
    const TabGroupChange& tab_group_change) {
  CHECK_EQ(tab_group_change.type, TabGroupChange::Type::kMoved);
  const TabGroup* tab_group =
      tab_group_change.model->group_model()->GetTabGroup(
          tab_group_change.group);

  auto event = mojom::OnNodeMovedEvent::New();
  event->id = NodeId(
      NodeId::Type::kCollection,
      base::NumberToString(tab_group->GetCollectionHandle().raw_value()));
  // The position of a group is defined by the index of its first tab.
  const gfx::Range tab_indices = tab_group->ListTabs();
  CHECK(!tab_indices.is_empty());
  // There is no start position for a TabGroup.
  event->from = tabs_api::Position(0);
  event->to = tabs_api::Position(tab_indices.start());
  return event;
}

mojom::OnCollectionCreatedEventPtr FromSplitTabToDataCreatedEvent(
    const SplitTabChange& split_tab_change) {
  auto event = mojom::OnCollectionCreatedEvent::New();
  const SplitTabChange::AddedChange* added_change =
      split_tab_change.GetAddedChange();
  CHECK(added_change);
  tabs::TabInterface* first_tab = added_change->tabs()[0].first;
  const tabs::TabCollection* split_collection =
      first_tab->GetParentCollection();
  event->data = tabs_api::converters::BuildMojoTabCollectionData(
      split_collection->GetHandle());
  event->position = tabs_api::Position(added_change->tabs()[0].second);
  return event;
}

}  // namespace tabs_api::events
