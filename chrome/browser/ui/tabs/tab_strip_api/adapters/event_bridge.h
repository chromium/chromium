// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EVENT_BRIDGE_H_

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_observer.h"

namespace tabs_api {

// Bridges the observation between the underlying model and the tab
// strip service. The tab strip service implements the events::EventObserver
// interface. The implementer is responsible for translating the platform
// specific event types to events::Event.
class EventBridge {
 public:
  virtual void AddObserver(events::EventObserver* observer) = 0;
  virtual void RemoveObserver(events::EventObserver* observer) = 0;

  virtual ~EventBridge() = default;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_EVENT_BRIDGE_H_
