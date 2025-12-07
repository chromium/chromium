// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace tabs_api {

namespace observation {

class TabStripApiBatchedObserver;

}  // namespace observation

// Simple tee which takes an event and broadcasts them to multiple targets.
class EventBroadcaster {
 public:
  EventBroadcaster() = default;
  EventBroadcaster(EventBroadcaster&&) = delete;
  EventBroadcaster& operator=(EventBroadcaster&) = delete;
  ~EventBroadcaster() = default;

  void Broadcast(const base::ObserverList<
                     observation::TabStripApiBatchedObserver>& targets,
                 const std::vector<events::Event>& event);
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_
