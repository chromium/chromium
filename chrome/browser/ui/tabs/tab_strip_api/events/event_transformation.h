// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tab_groups/tab_group_id.h"

namespace tabs_api::events {

// Utilities to convert external observation event types to native event::Event
// types. Unfortunately, external event types are not always easy to test, so
// some of the conversions are not covered by unit tests and must be covered in
// integration tests.

mojom::OnTabsCreatedEventPtr ToEvent(
    const tabs::TabHandle& handle,
    const tabs::TabCollection::Position& position,
    const tabs_api::TabStripModelAdapter* adapter);

mojom::OnCollectionCreatedEventPtr ToEvent(
    const tabs::TabCollectionHandle& handle,
    const tabs::TabCollection::Position& position,
    const tabs_api::TabStripModelAdapter* adapter,
    bool insert_from_detached);

mojom::OnTabsClosedEventPtr ToEvent(
    const tabs::TabCollectionNodes& removed_handles);

mojom::OnNodeMovedEventPtr ToEvent(
    const tabs::TabCollection::Position& to_position,
    const tabs::TabCollection::Position& from_position,
    const tabs::TabCollection::NodeHandle node_handle);

mojom::OnDataChangedEventPtr ToEvent(
    const tabs_api::TabStripModelAdapter* adapter,
    size_t index,
    TabChangeType change_type);

std::vector<Event> ToEvent(const TabStripSelectionChange& selection,
                           const tabs_api::TabStripModelAdapter* adapter);

mojom::OnDataChangedEventPtr ToEvent(const TabGroupChange& tab_group_change);

}  // namespace tabs_api::events

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
