// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_event_bridge.h"

namespace tabs_api::testing {

ToyTabStripEventBridge::ToyTabStripEventBridge(ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

ToyTabStripEventBridge::~ToyTabStripEventBridge() = default;

void ToyTabStripEventBridge::AddObserver(events::EventObserver* observer) {
  observers_.AddObserver(observer);
}

void ToyTabStripEventBridge::RemoveObserver(events::EventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ToyTabStripEventBridge::NotifyEvents(
    const std::vector<events::Event>& events) {
  for (auto& observer : observers_) {
    std::vector<events::Event> cloned_events;
    for (const auto& event : events) {
      cloned_events.push_back(std::visit(
          [](const auto& e) -> events::Event { return e.Clone(); }, event));
    }
    observer.OnEvents(std::move(cloned_events));
  }
}

}  // namespace tabs_api::testing
