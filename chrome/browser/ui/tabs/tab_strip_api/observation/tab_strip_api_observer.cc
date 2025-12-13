// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_observer.h"

namespace tabs_api::observation {

void TabStripApiObserver::OnTabEvents(
    const std::vector<mojom::TabsEventPtr>& events) {
  for (auto& event : events) {
    switch (event->which()) {
      case mojom::TabsEvent::Tag::kTabsCreatedEvent:
        OnTabsCreated(event->get_tabs_created_event());
        break;
      case mojom::TabsEvent::Tag::kTabsClosedEvent:
        OnTabsClosed(event->get_tabs_closed_event());
        break;
      case mojom::TabsEvent::Tag::kNodeMovedEvent:
        OnNodeMoved(event->get_node_moved_event());
        break;
      case mojom::TabsEvent::Tag::kDataChangedEvent:
        OnDataChanged(event->get_data_changed_event());
        break;
      case mojom::TabsEvent::Tag::kCollectionCreatedEvent:
        OnCollectionCreated(event->get_collection_created_event());
        break;
    }
  }
}

}  // namespace tabs_api::observation
