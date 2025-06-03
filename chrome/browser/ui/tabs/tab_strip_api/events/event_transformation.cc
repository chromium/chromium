// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"

namespace tabs_api::events {

mojom::OnTabsCreatedEventPtr ToEvent(const TabStripModelChange::Insert& insert,
                                     TabStripModel* tab_strip_model) {
  auto event = mojom::OnTabsCreatedEvent::New();
  for (auto& content : insert.contents) {
    auto tab_created = tabs_api::mojom::TabCreatedContainer::New();
    auto pos = tabs_api::mojom::Position::New();
    pos->index = content.index;
    tab_created->position = std::move(pos);
    auto renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, content.index);
    auto mojo_tab = tabs_api::converters::BuildMojoTab(content.tab->GetHandle(),
                                                       renderer_data);

    tab_created->tab = std::move(mojo_tab);
    event->tabs.emplace_back(std::move(tab_created));
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

mojom::OnTabDataChangedEventPtr ToEvent(
    const tabs_api::TabStripModelAdapter* adapter,
    size_t index,
    TabChangeType change_type) {
  auto event = mojom::OnTabDataChangedEvent::New();
  auto tabs = adapter->GetTabs();
  if (index < tabs.size()) {
    auto& handle = tabs.at(index);
    auto renderer_data = adapter->GetTabRendererData(index);
    event->tab = tabs_api::converters::BuildMojoTab(handle, renderer_data);
  }

  return event;
}

}  // namespace tabs_api::events
