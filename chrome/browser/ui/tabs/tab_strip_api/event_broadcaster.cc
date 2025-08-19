// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"

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

  mojom::TabsEventPtr operator()(const mojom::OnTabMovedEventPtr& event) {
    return mojom::TabsEvent::NewTabMovedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(const mojom::OnTabDataChangedEventPtr& event) {
    return mojom::TabsEvent::NewTabDataChangedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(
      const mojom::OnTabGroupCreatedEventPtr& event) {
    return mojom::TabsEvent::NewTabGroupCreatedEvent(event.Clone());
  }

  mojom::TabsEventPtr operator()(
      const mojom::OnTabGroupVisualsChangedEventPtr& event) {
    return mojom::TabsEvent::NewTabGroupVisualsChangedEvent(event.Clone());
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
    const mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver>& targets,
    const std::vector<events::Event>& events) {
  for (auto& target : targets) {
    target->OnTabEvents(Transform(events));
  }
}

}  // namespace tabs_api
