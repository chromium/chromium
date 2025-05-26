// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_H_

#include <variant>

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"

namespace tabs_api::events {

using Event =
    std::variant<mojom::OnTabsCreatedEventPtr, mojom::OnTabsClosedEventPtr>;

}

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_EVENT_H_
