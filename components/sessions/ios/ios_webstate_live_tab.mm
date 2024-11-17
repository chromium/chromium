// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_webstate_live_tab.h"

#include "base/memory/ptr_util.h"
#include "ios/web/public/navigation/navigation_manager.h"

namespace {
const char kIOSWebStateLiveTabWebStateUserDataKey[] = "ios_live_tab";
}

namespace sessions {

// static
IOSWebStateLiveTab* IOSWebStateLiveTab::GetForWebState(
    web::WebState* web_state) {
  if (!web_state->GetUserData(kIOSWebStateLiveTabWebStateUserDataKey)) {
    web_state->SetUserData(kIOSWebStateLiveTabWebStateUserDataKey,
                           base::WrapUnique(new IOSWebStateLiveTab(web_state)));
  }

  return static_cast<IOSWebStateLiveTab*>(
      web_state->GetUserData(kIOSWebStateLiveTabWebStateUserDataKey));
}

IOSWebStateLiveTab::IOSWebStateLiveTab(web::WebState* web_state)
    : web_state_(web_state) {}

IOSWebStateLiveTab::~IOSWebStateLiveTab() = default;

bool IOSWebStateLiveTab::IsInitialBlankNavigation() {
  return navigation_manager()->GetItemCount() == 0;
}

int IOSWebStateLiveTab::GetCurrentEntryIndex() {
  return navigation_manager()->GetLastCommittedItemIndex();
}

int IOSWebStateLiveTab::GetPendingEntryIndex() {
  return navigation_manager()->GetPendingItemIndex();
}

sessions::SerializedNavigationEntry IOSWebStateLiveTab::GetEntryAtIndex(
    int index) {
  return sessions::IOSSerializedNavigationBuilder::FromNavigationItem(
      index, *navigation_manager()->GetItemAtIndex(index));
}

sessions::SerializedNavigationEntry IOSWebStateLiveTab::GetPendingEntry() {
  return sessions::IOSSerializedNavigationBuilder::FromNavigationItem(
      GetPendingEntryIndex(), *navigation_manager()->GetPendingItem());
}

int IOSWebStateLiveTab::GetEntryCount() {
  return navigation_manager()->GetItemCount();
}

sessions::SerializedUserAgentOverride
IOSWebStateLiveTab::GetUserAgentOverride() {
  // Dynamic user agent overrides are not supported on iOS.
  return sessions::SerializedUserAgentOverride();
}

const web::WebState* IOSWebStateLiveTab::GetWebState() const {
  return web_state_;
}

}  // namespace sessions
