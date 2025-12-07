// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/interaction/browser_elements.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(BrowserElements);

BrowserElements::BrowserElements(BrowserWindowInterface& browser)
    : scoped_data_holder_(browser.GetUnownedUserDataHost(), *this) {}

BrowserElements::~BrowserElements() = default;

// static
BrowserElements* BrowserElements::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

ui::TrackedElement* BrowserElements::GetElement(ui::ElementIdentifier id) {
  return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
      id, GetContext());
}

BrowserElements::ElementList BrowserElements::GetAllElements(
    ui::ElementIdentifier id) {
  return ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
      id, GetContext());
}

bool BrowserElements::NotifyEvent(ui::ElementIdentifier id,
                                  ui::CustomElementEventType event_type) {
  auto* const element = GetElement(id);
  if (!element) {
    return false;
  }
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(element,
                                                                event_type);
  return true;
}
