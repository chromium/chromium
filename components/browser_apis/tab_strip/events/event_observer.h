// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_EVENTS_EVENT_OBSERVER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_EVENTS_EVENT_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/browser_apis/tab_strip/events/event.h"

namespace tabs_api::events {

// An observer for the tab strip api's internal representation of events. The
// only implementer of this should be the tab strip service itself. Platform
// adapters should translate their external representation of a tab mutation
// event to the component internal representation.
class EventObserver : public base::CheckedObserver {
 public:
  virtual void OnEvent(Event event) = 0;
  virtual void OnEvents(std::vector<Event> event) = 0;

  ~EventObserver() override = default;
};

}  // namespace tabs_api::events

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_EVENTS_EVENT_OBSERVER_H_
