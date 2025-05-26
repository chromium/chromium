// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"

namespace tabs_api::events {

mojom::OnTabsCreatedEventPtr ToEvent(
    const TabStripModelChange::Insert& insert) {
  auto event = mojom::OnTabsCreatedEvent::New();
  for (auto& content : insert.contents) {
    event->tabs.emplace_back(
        TabId::Type::kContent,
        base::NumberToString(content.tab->GetHandle().raw_value()));
  }
  return event;
}

mojom::OnTabsClosedEventPtr ToEvent(const TabStripModelChange::Remove& remove) {
  auto event = mojom::OnTabsClosedEvent::New();

  for (auto& content : remove.contents) {
    event->tabs.emplace_back(
        TabId::Type::kContent,
        base::NumberToString(content.tab->GetHandle().raw_value()));
  }

  return event;
}

}  // namespace tabs_api::events
