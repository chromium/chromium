// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"

#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"

namespace tabs_api {

// A decoder which takes the incoming event type and directs them to the proper
// callback.
class EventVisitor {
 public:
  EventVisitor() = default;

  mojom::TabsEventPtr operator()(const mojom::OnTabsCreatedEventPtr& event) {
    return mojom::TabsEvent::NewTabsCreatedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(const mojom::OnTabsClosedEventPtr& event) {
    return mojom::TabsEvent::NewTabsClosedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(const mojom::OnNodeMovedEventPtr& event) {
    return mojom::TabsEvent::NewNodeMovedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(const mojom::OnDataChangedEventPtr& event) {
    return mojom::TabsEvent::NewDataChangedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(
      const mojom::OnCollectionCreatedEventPtr& event) {
    return mojom::TabsEvent::NewCollectionCreatedEvent(event.Clone());
  }
};

std::vector<mojom::TabsEventPtr> Transform(
    const std::vector<events::Event>& events) {
  std::vector<mojom::TabsEventPtr> transformed;
  EventVisitor transformer;
  for (auto& event : events) {
    transformed.push_back(std::visit(transformer, event));
  }
  return transformed;
}

void EventBroadcaster::Broadcast(
    const base::ObserverList<observation::TabStripApiBatchedObserver>&
        observers,
    const std::vector<events::Event>& events) {
  for (auto& observer : observers) {
    observer.OnTabEvents(Transform(events));
  }
}

}  // namespace tabs_api
