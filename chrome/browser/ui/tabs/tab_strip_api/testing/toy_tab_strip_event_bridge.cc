// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_event_bridge.h"

namespace tabs_api::testing {

ToyTabStripEventBridge::ToyTabStripEventBridge(ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

ToyTabStripEventBridge::~ToyTabStripEventBridge() = default;

void ToyTabStripEventBridge::AddObserver(events::EventObserver* observer) {}

void ToyTabStripEventBridge::RemoveObserver(events::EventObserver* observer) {}

}  // namespace tabs_api::testing
