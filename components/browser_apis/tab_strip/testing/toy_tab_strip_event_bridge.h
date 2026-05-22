// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_

#include "base/observer_list.h"
#include "components/browser_apis/tab_strip/adapters/event_bridge.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"

namespace tabs_api::testing {

class ToyTabStripEventBridge : public EventBridge {
 public:
  explicit ToyTabStripEventBridge(ToyTabStrip* toy_tab_strip);
  ~ToyTabStripEventBridge() override;

  // EventBridge:
  void AddObserver(events::EventObserver* observer) override;
  void RemoveObserver(events::EventObserver* observer) override;

  void NotifyEvents(const std::vector<events::Event>& events);

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
  base::ObserverList<events::EventObserver> observers_;
};

}  // namespace tabs_api::testing

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_
