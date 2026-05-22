// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_UTILITIES_EVENT_FILTERS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_UTILITIES_EVENT_FILTERS_H_

#include <vector>

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"

namespace tabs_api::events {

// Note these utiliy methods return a non-owning pointers to the event objects.
//
std::vector<const tabs_api::mojom::OnTabsCreatedEvent*>
FilterForTabsCreatedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events);

std::vector<const tabs_api::mojom::OnNodesClosedEvent*>
FilterForNodesClosedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events);

std::vector<const tabs_api::mojom::OnNodeMovedEvent*> FilterForNodeMovedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events);

std::vector<const tabs_api::mojom::OnDataChangedEvent*>
FilterForDataChangedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events);

std::vector<const tabs_api::mojom::OnCollectionCreatedEvent*>
FilterForCollectionCreatedEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events);

}  // namespace tabs_api::events

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_UTILITIES_EVENT_FILTERS_H_
