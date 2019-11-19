// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_live_tab.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/navigation/navigation_manager.h"

namespace {
const char kIOSLiveTabWebStateUserDataKey[] = "ios_live_tab";
}

namespace sessions {

// static
IOSLiveTab* IOSLiveTab::GetForWebState(web::WebState* web_state) {
  if (!web_state->GetUserData(kIOSLiveTabWebStateUserDataKey)) {
    web_state->SetUserData(kIOSLiveTabWebStateUserDataKey,
                           base::WrapUnique(new IOSLiveTab(web_state)));
  }

  return static_cast<IOSLiveTab*>(
      web_state->GetUserData(kIOSLiveTabWebStateUserDataKey));
}

IOSLiveTab::IOSLiveTab(web::WebState* web_state) : web_state_(web_state) {}

IOSLiveTab::~IOSLiveTab() {}

bool IOSLiveTab::IsInitialBlankNavigation() {
  return navigation_manager()->GetItemCount() == 0;
}

int IOSLiveTab::GetCurrentEntryIndex() {
  return navigation_manager()->GetLastCommittedItemIndex();
}

int IOSLiveTab::GetPendingEntryIndex() {
  return navigation_manager()->GetPendingItemIndex();
}

sessions::SerializedNavigationEntry IOSLiveTab::GetEntryAtIndex(int index) {
  return sessions::IOSSerializedNavigationBuilder::FromNavigationItem(
      index, *navigation_manager()->GetItemAtIndex(index));
}

sessions::SerializedNavigationEntry IOSLiveTab::GetPendingEntry() {
  return sessions::IOSSerializedNavigationBuilder::FromNavigationItem(
      GetPendingEntryIndex(), *navigation_manager()->GetPendingItem());
}

int IOSLiveTab::GetEntryCount() {
  return navigation_manager()->GetItemCount();
}

const std::string& IOSLiveTab::GetUserAgentOverride() {
  // Dynamic user agent overrides are not supported on iOS.
  return user_agent_override_;
}

}  // namespace sessions
