// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/node_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"

namespace tabs_api::events {

mojom::OnTabsCreatedEventPtr ToEvent(const TabStripModelChange::Insert& insert,
                                     TabStripModel* tab_strip_model) {
  auto event = mojom::OnTabsCreatedEvent::New();
  for (auto& content : insert.contents) {
    auto tab_created = tabs_api::mojom::TabCreatedContainer::New();
    auto pos = tabs_api::Position(content.index);
    tab_created->position = std::move(pos);
    auto renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, content.index);
    auto mojo_tab = tabs_api::converters::BuildMojoTab(content.tab->GetHandle(),
                                                       renderer_data);

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

mojom::OnTabMovedEventPtr ToEvent(const TabStripModelChange::Move& move) {
  NodeId id(NodeId::Type::kContent,
            base::NumberToString(move.tab->GetHandle().raw_value()));

  auto from = tabs_api::Position(move.from_index);

  auto to = tabs_api::Position(move.to_index);

  auto event = mojom::OnTabMovedEvent::New();
  event->id = id;
  event->from = std::move(from);
  event->to = std::move(to);

  return event;
}

mojom::OnTabDataChangedEventPtr ToEvent(
    const tabs_api::TabStripModelAdapter* adapter,
    size_t index,
    TabChangeType change_type) {
  auto event = mojom::OnTabDataChangedEvent::New();
  auto tabs = adapter->GetTabs();
  if (index < tabs.size()) {
    auto& handle = tabs.at(index);
    auto renderer_data = adapter->GetTabRendererData(index);
    event->tab = tabs_api::converters::BuildMojoTab(handle, renderer_data);
  }

  return event;
}

mojom::OnTabGroupCreatedEventPtr ToTabGroupCreatedEvent(
    const TabGroupChange& tab_group_change) {
  CHECK_EQ(tab_group_change.type, TabGroupChange::Type::kCreated);
  TabGroup* tab_group = tab_group_change.model->group_model()->GetTabGroup(
      tab_group_change.group);
  auto event = mojom::OnTabGroupCreatedEvent::New();
  event->tab_collection = tabs_api::converters::BuildMojoTabCollection(
      tab_group->GetCollectionHandle());
  // TODO(crbug.com/412935315): Set the correct position.
  event->position =
      tabs_api::Position(0, NodeId::FromTabGroupId(tab_group_change.group));
  // When TabGroupChange::kCreated is fired, the TabGroupTabCollection is
  // empty. Then, TabGroupedStateChanged() is fired, which adds tabs to the
  // group.
  return event;
}

mojom::OnTabMovedEventPtr FromTabGroupedStateChangedToTabMovedEvent(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group_id,
    std::optional<tab_groups::TabGroupId> new_group_id,
    tabs::TabInterface* tab,
    int index) {
  auto event = mojom::OnTabMovedEvent::New();
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

mojom::OnTabGroupVisualsChangedEventPtr ToTabGroupVisualsChangedEvent(
    const TabGroupChange& tab_group_change) {
  CHECK_EQ(tab_group_change.type, TabGroupChange::Type::kVisualsChanged);
  TabGroup* tab_group = tab_group_change.model->group_model()->GetTabGroup(
      tab_group_change.group);
  auto event = mojom::OnTabGroupVisualsChangedEvent::New();
  event->tab_collection = tabs_api::converters::BuildMojoTabCollection(
      tab_group->GetCollectionHandle());
  return event;
}

}  // namespace tabs_api::events
