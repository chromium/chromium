// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_

#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace tabs_api::events {

// Utilities to convert external observation event types to native event::Event
// types. Unfortunately, external event types are not always easy to test, so
// some of the conversions are not covered by unit tests and must be covered in
// integration tests.

mojom::OnTabsCreatedEventPtr ToEvent(const TabStripModelChange::Insert& insert);
mojom::OnTabsClosedEventPtr ToEvent(const TabStripModelChange::Remove& remove);

}  // namespace tabs_api::events

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_TRANSFORMATION_H_
