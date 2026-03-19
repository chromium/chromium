// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

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

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_EVENT_BRIDGE_H_
