// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/event_filters.h"

namespace tabs_api::events {

std::vector<const tabs_api::mojom::OnTabsCreatedEvent*>
FilterForTabsCreatedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  std::vector<const tabs_api::mojom::OnTabsCreatedEvent*> created_events;
  for (const auto& event : events) {
    if (event->is_tabs_created_event()) {
      created_events.push_back(event->get_tabs_created_event().get());
    }
  }
  return created_events;
}

std::vector<const tabs_api::mojom::OnTabsClosedEvent*>
FilterForTabsClosedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  std::vector<const tabs_api::mojom::OnTabsClosedEvent*> closed_events;
  for (const auto& event : events) {
    if (event->is_tabs_closed_event()) {
      closed_events.push_back(event->get_tabs_closed_event().get());
    }
  }
  return closed_events;
}

std::vector<const tabs_api::mojom::OnNodeMovedEvent*> FilterForNodeMovedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  std::vector<const tabs_api::mojom::OnNodeMovedEvent*> moved_events;
  for (const auto& event : events) {
    if (event->is_node_moved_event()) {
      moved_events.push_back(event->get_node_moved_event().get());
    }
  }
  return moved_events;
}

std::vector<const tabs_api::mojom::OnDataChangedEvent*>
FilterForDataChangedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  std::vector<const tabs_api::mojom::OnDataChangedEvent*> changed_events;
  for (const auto& event : events) {
    if (event->is_data_changed_event()) {
      changed_events.push_back(event->get_data_changed_event().get());
    }
  }
  return changed_events;
}

std::vector<const tabs_api::mojom::OnCollectionCreatedEvent*>
FilterForCollectionCreatedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  std::vector<const tabs_api::mojom::OnCollectionCreatedEvent*> created_groups;
  for (const auto& event : events) {
    if (event->is_collection_created_event()) {
      created_groups.push_back(event->get_collection_created_event().get());
    }
  }
  return created_groups;
}

}  // namespace tabs_api::events
