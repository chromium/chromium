// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"

namespace tabs_api {

// A decoder which takes the incoming event type and directs them to the proper
// callback.
class EventVisitor {
 public:
  explicit EventVisitor(
      const mojo::AssociatedRemote<tabs_api::mojom::TabsObserver>* target)
      : target_(target) {}

  void operator()(mojom::OnTabsCreatedEventPtr& event) {
    (*target_)->OnTabsCreated(event.Clone());
  }

  void operator()(mojom::OnTabsClosedEventPtr& event) {
    (*target_)->OnTabsClosed(event.Clone());
  }

 private:
  raw_ptr<const mojo::AssociatedRemote<tabs_api::mojom::TabsObserver>> target_;
};

void EventBroadcaster::Broadcast(
    const mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver>& targets,
    events::Event& event) {
  for (auto& target : targets) {
    EventVisitor visitor(&target);
    std::visit(visitor, event);
  }
}

}  // namespace tabs_api
