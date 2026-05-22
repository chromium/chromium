// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_EVENT_BRIDGE_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_EVENT_BRIDGE_H_

#include "components/browser_apis/tab_strip/events/event_observer.h"

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

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_EVENT_BRIDGE_H_
