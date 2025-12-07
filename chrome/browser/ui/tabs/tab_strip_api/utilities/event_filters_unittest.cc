// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/event_filters.h"

#include "base/strings/string_number_conversions.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::events {
namespace {

TEST(EventFiltersTest, FilterEvents) {
  std::vector<tabs_api::mojom::TabsEventPtr> events;

  auto tabs_created_event = tabs_api::mojom::OnTabsCreatedEvent::New();
  tabs_created_event->tabs.push_back(
      tabs_api::mojom::TabCreatedContainer::New());
  tabs_created_event->tabs.push_back(
      tabs_api::mojom::TabCreatedContainer::New());
  events.push_back(tabs_api::mojom::TabsEvent::NewTabsCreatedEvent(
      std::move(tabs_created_event)));

  auto tabs_closed_event = tabs_api::mojom::OnTabsClosedEvent::New();
  tabs_closed_event->tabs.push_back(
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent, "111"));
  events.push_back(tabs_api::mojom::TabsEvent::NewTabsClosedEvent(
      std::move(tabs_closed_event)));

  auto tab_moved_event = tabs_api::mojom::OnNodeMovedEvent::New();
  tab_moved_event->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent, "222");
  events.push_back(tabs_api::mojom::TabsEvent::NewNodeMovedEvent(
      std::move(tab_moved_event)));

  auto tab_data_changed = tabs_api::mojom::OnDataChangedEvent::New();
  auto changed_tab = tabs_api::mojom::Tab::New();
  changed_tab->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent, "333");
  tab_data_changed->data =
      tabs_api::mojom::Data::NewTab(std::move(changed_tab));
  events.push_back(tabs_api::mojom::TabsEvent::NewDataChangedEvent(
      std::move(tab_data_changed)));

  auto group_data_changed = tabs_api::mojom::OnDataChangedEvent::New();
  auto changed_group = tabs_api::mojom::TabGroup::New();
  changed_group->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kCollection, "444");
  group_data_changed->data =
      tabs_api::mojom::Data::NewTabGroup(std::move(changed_group));
  events.push_back(tabs_api::mojom::TabsEvent::NewDataChangedEvent(
      std::move(group_data_changed)));

  auto created_event = tabs_api::mojom::OnCollectionCreatedEvent::New();
  events.push_back(tabs_api::mojom::TabsEvent::NewCollectionCreatedEvent(
      std::move(created_event)));

  auto created_tabs = FilterForTabsCreatedEvents(events);
  auto closed_tabs = FilterForTabsClosedEvents(events);
  auto moved_tabs = FilterForNodeMovedEvents(events);
  auto created_groups = FilterForCollectionCreatedEvents(events);
  auto data_changed_events = FilterForDataChangedEvents(events);

  EXPECT_EQ(created_tabs.size(), 1u);
  EXPECT_EQ(created_tabs[0]->tabs.size(), 2u);

  EXPECT_EQ(closed_tabs.size(), 1u);
  EXPECT_EQ(closed_tabs[0]->tabs[0].Id(), "111");

  EXPECT_EQ(moved_tabs.size(), 1u);
  EXPECT_EQ(moved_tabs[0]->id.Id(), "222");

  EXPECT_EQ(created_groups.size(), 1u);
  EXPECT_NE(created_groups[0], nullptr);

  ASSERT_TRUE(data_changed_events[0]->data->is_tab());
  EXPECT_EQ(data_changed_events[0]->data->get_tab()->id.Id(), "333");

  ASSERT_TRUE(data_changed_events[1]->data->is_tab_group());
  EXPECT_EQ(data_changed_events[1]->data->get_tab_group()->id.Id(), "444");
}

}  // namespace
}  // namespace tabs_api::events
