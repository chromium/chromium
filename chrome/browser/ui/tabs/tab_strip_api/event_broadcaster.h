// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_

#include "chrome/browser/ui/tabs/tab_strip_api//events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace tabs_api {

// Simple tee which takes an event and broadcasts them to multiple targets.
class EventBroadcaster {
 public:
  EventBroadcaster() = default;
  EventBroadcaster(EventBroadcaster&&) = delete;
  EventBroadcaster& operator=(EventBroadcaster&) = delete;
  ~EventBroadcaster() = default;

  void Broadcast(
      const mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver>& targets,
      events::Event& event);
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENT_BROADCASTER_H_
