// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"

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

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_OBSERVER_H_
