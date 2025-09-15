// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"

namespace tabs_api::events {

// Utilities to convert external observation event types to native event::Event
// types. Unfortunately, external event types are not always easy to test, so
// some of the conversions are not covered by unit tests and must be covered in
// integration tests.

mojom::OnTabsCreatedEventPtr ToEvent(
    const TabStripModelChange::Insert& insert,
    const tabs_api::TabStripModelAdapter* adapter);
mojom::OnTabsClosedEventPtr ToEvent(const TabStripModelChange::Remove& remove);
mojom::OnNodeMovedEventPtr ToEvent(
    const TabStripModelChange::Move& move,
    const tabs_api::TabStripModelAdapter* adapter);
mojom::OnDataChangedEventPtr ToEvent(
    const tabs_api::TabStripModelAdapter* adapter,
    size_t index,
    TabChangeType change_type);
std::vector<Event> ToEvent(const TabStripSelectionChange& selection,
                           const tabs_api::TabStripModelAdapter* adapter);

// When a tab group is opened, there're multiple events fired from
// TabStripModelObserver. The following functions convert them to TabStripAPI
// events.
// 1. TabGroupChange with type kCreated => OnTabGroupCreatedEvent
//    This event is fired when a tab group is created. At this point, the
//    TabGroupTabCollection and the visual data are empty.
// 2. TabGroupChange with type kVisualsChanged => OnTabGroupVisualsChangedEvent
//    This event is fired when the visual data (color, title, etc.) of a tab
//    group is changed.
// 3. TabGroupedStateChanged() => OnNodeMovedEvent
//    this event updates the affiliation of a tab with a group.
mojom::OnCollectionCreatedEventPtr FromTabGroupToDataCreatedEvent(
    const TabGroupChange& tab_group_change);

mojom::OnNodeMovedEventPtr FromTabGroupedStateChangedToNodeMovedEvent(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index);
mojom::OnDataChangedEventPtr ToEvent(const TabGroupChange& tab_group_change);

mojom::OnNodeMovedEventPtr ToTabGroupMovedEvent(
    const TabGroupChange& tab_group_change);

mojom::OnCollectionCreatedEventPtr FromSplitTabToDataCreatedEvent(
    const SplitTabChange& split_tab_change);

}  // namespace tabs_api::events

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
